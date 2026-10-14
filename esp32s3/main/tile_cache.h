#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHAN_SHUI_TILE_WIDTH 800
#define SHAN_SHUI_TILE_COUNT 4

typedef struct {
  volatile int32_t world_tile;
  volatile bool ready;
  uint16_t *pixels;
} shan_shui_tile_t;

typedef struct {
  uint32_t seed;
  shan_shui_tile_t tiles[SHAN_SHUI_TILE_COUNT];
  TaskHandle_t render_task;
  volatile int32_t requested_world_tile;
  int32_t rendered_world_tile;
} shan_shui_tile_cache_t;

bool shan_shui_tile_cache_init(shan_shui_tile_cache_t *cache, uint32_t seed);
void shan_shui_tile_cache_compose(shan_shui_tile_cache_t *cache, uint16_t *framebuffer, int32_t camera_x);
