#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

typedef struct {
  esp_lcd_panel_handle_t panel;
  uint16_t *framebuffer;
} shan_shui_display_t;

esp_err_t shan_shui_display_init(shan_shui_display_t *display);

/*
 * The Waveshare 4.3C backlight is driven through its board I/O expander.
 * The first bring-up keeps that board policy separate from RGB transport.
 */
esp_err_t shan_shui_board_backlight_enable(void);
