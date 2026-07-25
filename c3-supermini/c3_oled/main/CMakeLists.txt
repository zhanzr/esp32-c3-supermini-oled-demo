idf_component_register(SRCS "main.c" "oled.c" "oledfont.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c esp_driver_gpio freertos)
