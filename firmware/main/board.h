// Mapa de pinos e periféricos do terminal (ESP32-S3-DevKitC-1 no Wokwi).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C compartilhado: OLED SSD1306 (0x3C) + RTC DS1307 (0x68)
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define OLED_ADDR 0x3C
#define DS1307_ADDR 0x68

// Teclado matricial 4x4 (linhas = saídas, colunas = entradas com pull-up)
#define PIN_ROW1 4
#define PIN_ROW2 5
#define PIN_ROW3 6
#define PIN_ROW4 7
#define PIN_COL1 15
#define PIN_COL2 16
#define PIN_COL3 17
#define PIN_COL4 18

#define PIN_LED_OK 10     // verde: aprovada
#define PIN_LED_FRAUD 11  // vermelho: bloqueada
#define PIN_BUZZER 12

i2c_master_bus_handle_t board_i2c_init(void);
void board_io_init(void);
void board_leds(bool ok, bool fraud);
void board_beep(int freq_hz, int ms);  // bloqueante (curto)

#ifdef __cplusplus
}
#endif
