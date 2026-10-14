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
static const int kScrollPixelsPerSecond = 20;

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
  int64_t last_stats_at = started_at;
  uint32_t last_dma_frames = 0;
  uint32_t last_presented_frames = 0;
  int32_t last_presented_camera_x = -1;
  unsigned int draw_framebuffer = 1;
  ESP_LOGI(TAG, "scrolling procedural tile preview at %d px/s", kScrollPixelsPerSecond);
  while (true) {
    shan_shui_display_wait_for_free_framebuffer();
    const int64_t elapsed_us = esp_timer_get_time() - started_at;
    const int32_t camera_x = (int32_t)((elapsed_us * kScrollPixelsPerSecond) / 1000000);
    if (camera_x == last_presented_camera_x) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    const int64_t compose_started_at = esp_timer_get_time();
    shan_shui_tile_cache_compose(&tiles, display.framebuffers[draw_framebuffer], camera_x);
    const int64_t compose_us = esp_timer_get_time() - compose_started_at;
    ESP_ERROR_CHECK(shan_shui_display_present(&display, draw_framebuffer));
    last_presented_camera_x = camera_x;
    draw_framebuffer = (draw_framebuffer + 1) % SHAN_SHUI_FRAMEBUFFER_COUNT;

    const int64_t now = esp_timer_get_time();
    if (now - last_stats_at >= 1000000) {
      shan_shui_display_stats_t stats;
      shan_shui_display_get_stats(&stats);
      ESP_LOGI(TAG, "display: DMA %u fps, present %u fps, compose %lld us, camera %ld",
               stats.dma_frames_completed - last_dma_frames, stats.frames_presented - last_presented_frames,
               (long long)compose_us, (long)camera_x);
      last_dma_frames = stats.dma_frames_completed;
      last_presented_frames = stats.frames_presented;
      last_stats_at = now;
    }
    /* A full PSRAM composition can overrun a refresh interval; let IDLE0 run. */
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
