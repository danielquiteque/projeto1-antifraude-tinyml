// Parte dependente de hardware: barramento I2C do ESP-IDF (driver i2c_master, IDF >= 5.2).
#include <string.h>

#include "esp_check.h"
#include "ssd1306.h"

static const char *TAG = "ssd1306";

static esp_err_t cmd(ssd1306_t *d, const uint8_t *c, size_t n) {
    uint8_t buf[32];
    buf[0] = 0x00;  // byte de controle: Co=0, D/C#=0 → comandos
    memcpy(&buf[1], c, n);
    return i2c_master_transmit((i2c_master_dev_handle_t)d->dev, buf, n + 1, 100);
}

esp_err_t ssd1306_init(ssd1306_t *d, i2c_master_bus_handle_t bus, uint8_t addr) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t h;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &h), TAG, "add device");
    d->dev = h;
    static const uint8_t init_seq[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock
        0xA8, 0x3F,  // multiplex 64
        0xD3, 0x00,  // offset
        0x40,        // start line 0
        0x8D, 0x14,  // charge pump on
        0x20, 0x00,  // endereçamento horizontal
        0xA1, 0xC8,  // espelhamento (orientação padrão dos módulos)
        0xDA, 0x12,  // COM pins
        0x81, 0xCF,  // contraste
        0xD9, 0xF1,  // pre-charge
        0xDB, 0x40,  // VCOMH
        0xA4, 0xA6,  // RAM → display, modo normal
        0xAF,        // display on
    };
    ESP_RETURN_ON_ERROR(cmd(d, init_seq, sizeof(init_seq)), TAG, "init");
    ssd1306_clear(d);
    return ssd1306_flush(d);
}

esp_err_t ssd1306_flush(ssd1306_t *d) {
    static const uint8_t win[] = {0x21, 0, SSD1306_WIDTH - 1, 0x22, 0, SSD1306_HEIGHT / 8 - 1};
    ESP_RETURN_ON_ERROR(cmd(d, win, sizeof(win)), TAG, "window");
    static uint8_t buf[1 + sizeof(d->fb)];
    buf[0] = 0x40;  // byte de controle: dados de RAM
    memcpy(&buf[1], d->fb, sizeof(d->fb));
    return i2c_master_transmit((i2c_master_dev_handle_t)d->dev, buf, sizeof(buf), 200);
}
