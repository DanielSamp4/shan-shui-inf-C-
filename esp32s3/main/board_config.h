#pragma once

/*
 * Waveshare ESP32-S3-Touch-LCD-4.3C / ESP32-S3-WROOM-1-N16R8 baseline.
 * Keep every board-specific value here. Confirm this mapping against the
 * schematic of the physical board before changing electrical parameters.
 */
#define SHAN_SHUI_LCD_WIDTH 800
#define SHAN_SHUI_LCD_HEIGHT 480
#define SHAN_SHUI_LCD_BPP 16
#define SHAN_SHUI_LCD_PCLK_HZ (16 * 1000 * 1000)

#define SHAN_SHUI_PIN_VSYNC 3
#define SHAN_SHUI_PIN_HSYNC 46
#define SHAN_SHUI_PIN_DE 5
#define SHAN_SHUI_PIN_PCLK 7

/* B3..B7, G2..G7, R3..R7: physical RGB565 data bus ordering. */
#define SHAN_SHUI_RGB_DATA_PINS \
  {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40}

/* Timing validated by the Waveshare 4.3C example family; tune only on board. */
#define SHAN_SHUI_HSYNC_BACK_PORCH 40
#define SHAN_SHUI_HSYNC_FRONT_PORCH 20
#define SHAN_SHUI_HSYNC_PULSE_WIDTH 1
#define SHAN_SHUI_VSYNC_BACK_PORCH 8
#define SHAN_SHUI_VSYNC_FRONT_PORCH 4
#define SHAN_SHUI_VSYNC_PULSE_WIDTH 1
