#include "display.h"

#include "board_config.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "shan_shui.display";
static TaskHandle_t s_presentation_task;
static volatile uint32_t s_dma_frames_completed;
static volatile uint32_t s_frames_presented;

static IRAM_ATTR bool shan_shui_display_on_frame_buffer_complete(esp_lcd_panel_handle_t panel,
                                                                 const esp_lcd_rgb_panel_event_data_t *edata,
                                                                 void *user_ctx) {
  (void)panel;
  (void)edata;
  (void)user_ctx;
  ++s_dma_frames_completed;
  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(s_presentation_task, &higher_priority_task_woken);
  return higher_priority_task_woken == pdTRUE;
}

esp_err_t shan_shui_board_backlight_enable(void) {
  /*
   * TODO(board): Port the Waveshare I2C expander driver for the exact board
   * revision. Do not drive a guessed ESP GPIO: 4.3C routes backlight through
   * its I/O expander. Keeping this explicit prevents accidental pin damage.
   */
  ESP_LOGW(TAG, "backlight requires the Waveshare I2C expander port");
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t shan_shui_display_init(shan_shui_display_t *display) {
  if (display == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const int data_pins[16] = SHAN_SHUI_RGB_DATA_PINS;
  esp_lcd_rgb_panel_config_t config = {
      .clk_src = LCD_CLK_SRC_DEFAULT,
      .data_width = 16,
      .in_color_format = LCD_COLOR_FMT_RGB565,
      .out_color_format = LCD_COLOR_FMT_RGB565,
      .num_fbs = SHAN_SHUI_FRAMEBUFFER_COUNT,
      .disp_gpio_num = -1,
      .pclk_gpio_num = SHAN_SHUI_PIN_PCLK,
      .vsync_gpio_num = SHAN_SHUI_PIN_VSYNC,
      .hsync_gpio_num = SHAN_SHUI_PIN_HSYNC,
      .de_gpio_num = SHAN_SHUI_PIN_DE,
      .data_gpio_nums = {
          data_pins[0], data_pins[1], data_pins[2], data_pins[3],
          data_pins[4], data_pins[5], data_pins[6], data_pins[7],
          data_pins[8], data_pins[9], data_pins[10], data_pins[11],
          data_pins[12], data_pins[13], data_pins[14], data_pins[15],
      },
      .timings = {
          .pclk_hz = SHAN_SHUI_LCD_PCLK_HZ,
          .h_res = SHAN_SHUI_LCD_WIDTH,
          .v_res = SHAN_SHUI_LCD_HEIGHT,
          .hsync_back_porch = SHAN_SHUI_HSYNC_BACK_PORCH,
          .hsync_front_porch = SHAN_SHUI_HSYNC_FRONT_PORCH,
          .hsync_pulse_width = SHAN_SHUI_HSYNC_PULSE_WIDTH,
          .vsync_back_porch = SHAN_SHUI_VSYNC_BACK_PORCH,
          .vsync_front_porch = SHAN_SHUI_VSYNC_FRONT_PORCH,
          .vsync_pulse_width = SHAN_SHUI_VSYNC_PULSE_WIDTH,
          .flags.pclk_active_neg = false,
      },
      .flags.fb_in_psram = true,
  };

  ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &display->panel), TAG, "create RGB panel");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(display->panel), TAG, "reset panel");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_init(display->panel), TAG, "init panel");
  /* RGB panels do not implement esp_lcd_panel_disp_on_off(). */
  ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(display->panel, SHAN_SHUI_FRAMEBUFFER_COUNT,
                                                          (void **)&display->framebuffers[0],
                                                          (void **)&display->framebuffers[1],
                                                          (void **)&display->framebuffers[2]),
                      TAG, "get framebuffers");
  s_presentation_task = xTaskGetCurrentTaskHandle();
  const esp_lcd_rgb_panel_event_callbacks_t callbacks = {
      .on_frame_buf_complete = shan_shui_display_on_frame_buffer_complete,
  };
  ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(display->panel, &callbacks, NULL), TAG,
                      "register framebuffer callback");
  ESP_LOGI(TAG, "RGB565: %dx%d, %u bytes per framebuffer, triple buffered", SHAN_SHUI_LCD_WIDTH,
           SHAN_SHUI_LCD_HEIGHT, SHAN_SHUI_LCD_WIDTH * SHAN_SHUI_LCD_HEIGHT * 2);
  return ESP_OK;
}

void shan_shui_display_wait_for_free_framebuffer(void) {
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

esp_err_t shan_shui_display_present(shan_shui_display_t *display, unsigned int framebuffer_index) {
  if (display == NULL || framebuffer_index >= SHAN_SHUI_FRAMEBUFFER_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  const esp_err_t result = esp_lcd_panel_draw_bitmap(display->panel, 0, 0, SHAN_SHUI_LCD_WIDTH,
                                                      SHAN_SHUI_LCD_HEIGHT, display->framebuffers[framebuffer_index]);
  if (result == ESP_OK) {
    ++s_frames_presented;
  }
  return result;
}

void shan_shui_display_get_stats(shan_shui_display_stats_t *stats) {
  if (stats == NULL) {
    return;
  }
  stats->dma_frames_completed = s_dma_frames_completed;
  stats->frames_presented = s_frames_presented;
}
