#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define SHAN_SHUI_FRAMEBUFFER_COUNT 3

typedef struct {
  esp_lcd_panel_handle_t panel;
  uint16_t *framebuffers[SHAN_SHUI_FRAMEBUFFER_COUNT];
} shan_shui_display_t;

typedef struct {
  uint32_t dma_frames_completed;
  uint32_t frames_presented;
} shan_shui_display_stats_t;

esp_err_t shan_shui_display_init(shan_shui_display_t *display);

/* Blocks until the RGB DMA has finished reading a framebuffer. */
void shan_shui_display_wait_for_free_framebuffer(void);

/* Makes a fully composed framebuffer visible on the next RGB DMA frame. */
esp_err_t shan_shui_display_present(shan_shui_display_t *display, unsigned int framebuffer_index);
void shan_shui_display_get_stats(shan_shui_display_stats_t *stats);

/*
 * The Waveshare 4.3C backlight is driven through its board I/O expander.
 * The first bring-up keeps that board policy separate from RGB transport.
 */
esp_err_t shan_shui_board_backlight_enable(void);
