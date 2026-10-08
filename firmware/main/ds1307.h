// RTC DS1307 (I2C). Fornece a hora da transação — feature de hora do dia e Δt entre compras.
#pragma once
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int year, month, day, hour, minute, second;
} rtc_time_t;

esp_err_t ds1307_init(i2c_master_bus_handle_t bus, uint8_t addr);
esp_err_t ds1307_read(rtc_time_t *t);
esp_err_t ds1307_write(const rtc_time_t *t);

// Conversões calendário <-> segundos desde 1970 (sem fuso: o RTC guarda a hora local).
uint32_t rtc_to_unix(const rtc_time_t *t);
void rtc_from_unix(uint32_t s, rtc_time_t *t);

#ifdef __cplusplus
}
#endif
