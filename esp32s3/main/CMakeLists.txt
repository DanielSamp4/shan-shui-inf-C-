idf_component_register(
  SRCS "main.c" "display.c" "tile_cache.c"
  INCLUDE_DIRS "."
  REQUIRES esp_lcd esp_timer
)
