idf_component_register(
    SRCS "main.c" "ws2812_led.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_gpio esp_driver_rmt
)
