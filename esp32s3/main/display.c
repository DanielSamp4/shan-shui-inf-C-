#include "display.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"

static const char *TAG = "shan_shui.display";

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
      .bits_per_pixel = SHAN_SHUI_LCD_BPP,
      .num_fbs = 1,
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
  ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(display->panel, true), TAG, "enable panel");
  ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(display->panel, 1, (void **)&display->framebuffer), TAG,
                      "get framebuffer");
  ESP_LOGI(TAG, "RGB565 framebuffer: %dx%d, %u bytes", SHAN_SHUI_LCD_WIDTH, SHAN_SHUI_LCD_HEIGHT,
           SHAN_SHUI_LCD_WIDTH * SHAN_SHUI_LCD_HEIGHT * 2);
  return ESP_OK;
}
