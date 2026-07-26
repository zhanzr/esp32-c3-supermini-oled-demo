idf_component_register(
    SRCS "main.c" "ws2812_led.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash driver esp_driver_gpio esp_driver_rmt freertos
)
