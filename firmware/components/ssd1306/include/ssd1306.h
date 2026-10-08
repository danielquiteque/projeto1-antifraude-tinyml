// Driver mínimo para OLED SSD1306 128x64 via I2C (ESP-IDF, driver i2c_master).
// O desenho é feito num framebuffer em RAM (1 KB) e enviado de uma vez com ssd1306_flush().
// As funções de desenho não dependem de hardware (podem ser testadas no PC).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

typedef struct {
    uint8_t fb[SSD1306_WIDTH * SSD1306_HEIGHT / 8];  // 1 bit por pixel, páginas de 8 linhas
    void *dev;                                       // i2c_master_dev_handle_t (NULL no teste em PC)
} ssd1306_t;

#ifdef ESP_PLATFORM
#include "driver/i2c_master.h"
#include "esp_err.h"
// Adiciona o display ao barramento I2C já criado e envia a sequência de inicialização.
esp_err_t ssd1306_init(ssd1306_t *d, i2c_master_bus_handle_t bus, uint8_t addr);
// Envia o framebuffer inteiro para o display.
esp_err_t ssd1306_flush(ssd1306_t *d);
#endif

void ssd1306_clear(ssd1306_t *d);
void ssd1306_pixel(ssd1306_t *d, int x, int y, bool on);
void ssd1306_hline(ssd1306_t *d, int x, int y, int w);
void ssd1306_line(ssd1306_t *d, int x0, int y0, int x1, int y1);
void ssd1306_rect(ssd1306_t *d, int x, int y, int w, int h, bool fill);
// Texto com fonte 5x7 (6 px por caractere). scale=1 → 21 colunas x 8 linhas; scale=2 → 10 x 4.
// Retorna a coordenada x após o último caractere.
int ssd1306_text(ssd1306_t *d, int x, int y, const char *s, int scale);
// Inverte (vídeo reverso) um retângulo — útil para destacar a decisão.
void ssd1306_invert(ssd1306_t *d, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif
