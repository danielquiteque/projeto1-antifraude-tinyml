#include "ds1307.h"

#include "esp_check.h"

static const char *TAG = "ds1307";
static i2c_master_dev_handle_t s_dev;

static int bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

esp_err_t ds1307_init(i2c_master_bus_handle_t bus, uint8_t addr) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG, "add device");
    // Garante que o oscilador está ligado (bit CH do registrador 0 = 0).
    uint8_t reg = 0x00, sec;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_dev, &reg, 1, &sec, 1, 100), TAG, "read");
    if (sec & 0x80) {
        uint8_t w[2] = {0x00, (uint8_t)(sec & 0x7F)};
        ESP_RETURN_ON_ERROR(i2c_master_transmit(s_dev, w, 2, 100), TAG, "start osc");
    }
    return ESP_OK;
}

esp_err_t ds1307_read(rtc_time_t *t) {
    uint8_t reg = 0x00, b[7];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_dev, &reg, 1, b, 7, 100), TAG, "read");
    t->second = bcd2dec(b[0] & 0x7F);
    t->minute = bcd2dec(b[1] & 0x7F);
    t->hour = bcd2dec(b[2] & 0x3F);  // modo 24 h
    t->day = bcd2dec(b[4] & 0x3F);
    t->month = bcd2dec(b[5] & 0x1F);
    t->year = 2000 + bcd2dec(b[6]);
    return ESP_OK;
}

esp_err_t ds1307_write(const rtc_time_t *t) {
    uint8_t w[8] = {0x00,
                    dec2bcd(t->second),
                    dec2bcd(t->minute),
                    dec2bcd(t->hour),
                    1,
                    dec2bcd(t->day),
                    dec2bcd(t->month),
                    dec2bcd(t->year % 100)};
    return i2c_master_transmit(s_dev, w, sizeof(w), 100);
}

// Algoritmo "days from civil" (H. Hinnant), válido para o calendário gregoriano.
static int32_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = y / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

uint32_t rtc_to_unix(const rtc_time_t *t) {
    return (uint32_t)days_from_civil(t->year, t->month, t->day) * 86400u + (uint32_t)(t->hour * 3600 + t->minute * 60 + t->second);
}

void rtc_from_unix(uint32_t s, rtc_time_t *t) {
    int32_t z = (int32_t)(s / 86400u) + 719468;
    uint32_t rem = s % 86400u;
    int era = z / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    t->day = doy - (153 * mp + 2) / 5 + 1;
    t->month = mp < 10 ? mp + 3 : mp - 9;
    t->year = yoe + era * 400 + (t->month <= 2);
    t->hour = (int)(rem / 3600);
    t->minute = (int)(rem % 3600 / 60);
    t->second = (int)(rem % 60);
}
