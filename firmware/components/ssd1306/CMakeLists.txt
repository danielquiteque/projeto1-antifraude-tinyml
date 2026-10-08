idf_component_register(SRCS "ssd1306_gfx.c" "ssd1306_i2c.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2c)
