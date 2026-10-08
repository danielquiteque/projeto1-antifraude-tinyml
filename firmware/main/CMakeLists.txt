idf_component_register(
    SRCS "main.cc" "fraud_model.cc" "fraud_features.c" "model_data.cc"
         "board.c" "keypad.c" "ds1307.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ssd1306 net esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_timer)
# esp-tflite-micro entra pelo idf_component.yml (Component Manager)
