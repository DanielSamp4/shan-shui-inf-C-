#include <stdint.h>

#include "board_config.h"
#include "display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tile_cache.h"

static const char *TAG = "shan_shui";
static const uint32_t kSeed = 1208931029;
static const int kScrollPixelsPerSecond = 60;

void app_main(void) {
  shan_shui_display_t display = {0};
  ESP_ERROR_CHECK(shan_shui_display_init(&display));

  /* Backlight is board-expander specific; warning is intentional during bring-up. */
  const esp_err_t backlight = shan_shui_board_backlight_enable();
  if (backlight != ESP_OK) {
    ESP_LOGW(TAG, "RGB started; port board backlight before physical display validation");
  }

  shan_shui_tile_cache_t tiles;
  if (!shan_shui_tile_cache_init(&tiles, kSeed)) {
    ESP_LOGE(TAG, "not enough PSRAM for tile cache");
    return;
  }

  const int64_t started_at = esp_timer_get_time();
  ESP_LOGI(TAG, "scrolling procedural tile preview at %d px/s", kScrollPixelsPerSecond);
  while (true) {
    const int64_t elapsed_us = esp_timer_get_time() - started_at;
    const int32_t camera_x = (int32_t)((elapsed_us * kScrollPixelsPerSecond) / 1000000);
    shan_shui_tile_cache_compose(&tiles, display.framebuffer, camera_x);
    vTaskDelay(pdMS_TO_TICKS(33));  // initial 30 Hz target; no partial tile is presented
  }
}
