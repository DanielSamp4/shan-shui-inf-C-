#include "tile_cache.h"

#include <stdbool.h>
#include <string.h>

#include "board_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "shan_shui.tiles";

static uint32_t hash32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352d;
  value ^= value >> 15;
  value *= 0x846ca68b;
  return value ^ (value >> 16);
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xf8) << 8) | ((uint16_t)(g & 0xfc) << 3) | (b >> 3);
}

/* Temporary lightweight scene. Its contract is a tile, not a full screenshot. */
static void raster_preview_tile(shan_shui_tile_t *tile, uint32_t seed) {
  const int32_t origin_x = tile->world_tile * SHAN_SHUI_TILE_WIDTH;
  for (int y = 0; y < SHAN_SHUI_LCD_HEIGHT; ++y) {
    for (int x = 0; x < SHAN_SHUI_TILE_WIDTH; ++x) {
      const int32_t world_x = origin_x + x;
      const uint32_t grain = hash32(seed ^ (uint32_t)(world_x * 733 + y * 991));
      const uint8_t paper = 225 + (grain & 7);
      uint16_t color = rgb565(paper, paper - 5, paper - 16);

      const uint32_t ridge = hash32(seed + (uint32_t)(world_x / 5));
      const int horizon = 300 - (int)(ridge % 115) + (world_x % 73) / 5;
      if (y > horizon) {
        const uint8_t ink = (uint8_t)(52 + ((ridge >> 9) & 31));
        color = rgb565(ink, ink + 5, ink);
        if (((world_x + y * 3) % 31) < 2 && y > horizon + 14) {
          color = rgb565(38, 43, 38);  // early hatching proxy
        }
      }
      tile->pixels[y * SHAN_SHUI_TILE_WIDTH + x] = color;
    }
    /* Tile construction runs during bring-up on the presentation task. */
    if ((y & 15) == 15) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

static void render_tile(shan_shui_tile_cache_t *cache, int32_t world_tile) {
  shan_shui_tile_t *tile = &cache->tiles[(uint32_t)world_tile % SHAN_SHUI_TILE_COUNT];
  tile->ready = false;
  tile->world_tile = world_tile;
  raster_preview_tile(tile, cache->seed);
  tile->ready = true;
}

static void tile_render_task(void *arg) {
  shan_shui_tile_cache_t *cache = arg;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (cache->rendered_world_tile < cache->requested_world_tile) {
      const int32_t world_tile = ++cache->rendered_world_tile;
      render_tile(cache, world_tile);
      ESP_LOGD(TAG, "prepared tile %ld", (long)world_tile);
    }
  }
}

static void request_tile_through(shan_shui_tile_cache_t *cache, int32_t world_tile) {
  if (world_tile <= cache->requested_world_tile) {
    return;
  }
  cache->requested_world_tile = world_tile;
  xTaskNotifyGive(cache->render_task);
}

static shan_shui_tile_t *find_tile(shan_shui_tile_cache_t *cache, int32_t world_tile) {
  shan_shui_tile_t *tile = &cache->tiles[(uint32_t)world_tile % SHAN_SHUI_TILE_COUNT];
  while (!tile->ready || tile->world_tile != world_tile) {
    ESP_LOGW(TAG, "waiting for tile %ld", (long)world_tile);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return tile;
}

bool shan_shui_tile_cache_init(shan_shui_tile_cache_t *cache, uint32_t seed) {
  memset(cache, 0, sizeof(*cache));
  cache->seed = seed;
  const size_t bytes = SHAN_SHUI_TILE_WIDTH * SHAN_SHUI_LCD_HEIGHT * sizeof(uint16_t);
  for (int i = 0; i < SHAN_SHUI_TILE_COUNT; ++i) {
    cache->tiles[i].world_tile = INT32_MIN;
    cache->tiles[i].ready = false;
    cache->tiles[i].pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (cache->tiles[i].pixels == NULL) {
      ESP_LOGE(TAG, "PSRAM allocation failed for tile %d", i);
      return false;
    }
  }
  /* The current screen and the immediately following screen are ready at boot. */
  for (int32_t world_tile = 0; world_tile < 2; ++world_tile) {
    render_tile(cache, world_tile);
  }
  cache->rendered_world_tile = 1;
  cache->requested_world_tile = 1;
  if (xTaskCreatePinnedToCore(tile_render_task, "shan_shui_tiles", 4096, cache, 4, &cache->render_task, 1) != pdPASS) {
    ESP_LOGE(TAG, "could not create tile worker");
    return false;
  }
  ESP_LOGI(TAG, "tile cache: %d tiles × %u bytes, worker on CPU1", SHAN_SHUI_TILE_COUNT, (unsigned)bytes);
  return true;
}

void shan_shui_tile_cache_compose(shan_shui_tile_cache_t *cache, uint16_t *framebuffer, int32_t camera_x) {
  const int32_t first_tile = camera_x / SHAN_SHUI_TILE_WIDTH;
  const int32_t in_tile_x = camera_x % SHAN_SHUI_TILE_WIDTH;
  /* Keep two whole tiles ahead of the widest possible 800 px viewport. */
  request_tile_through(cache, first_tile + SHAN_SHUI_TILE_COUNT - 1);
  for (int x = 0; x < SHAN_SHUI_LCD_WIDTH;) {
    const int32_t absolute_x = in_tile_x + x;
    const int32_t tile_index = first_tile + absolute_x / SHAN_SHUI_TILE_WIDTH;
    const int32_t source_x = absolute_x % SHAN_SHUI_TILE_WIDTH;
    const int count = SHAN_SHUI_TILE_WIDTH - source_x < SHAN_SHUI_LCD_WIDTH - x
                          ? SHAN_SHUI_TILE_WIDTH - source_x
                          : SHAN_SHUI_LCD_WIDTH - x;
    shan_shui_tile_t *tile = find_tile(cache, tile_index);
    for (int y = 0; y < SHAN_SHUI_LCD_HEIGHT; ++y) {
      memcpy(&framebuffer[y * SHAN_SHUI_LCD_WIDTH + x], &tile->pixels[y * SHAN_SHUI_TILE_WIDTH + source_x],
             (size_t)count * sizeof(uint16_t));
    }
    x += count;
  }
}
