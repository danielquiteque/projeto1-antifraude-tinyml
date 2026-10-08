#include "keypad.h"

#include "board.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const int kRows[4] = {PIN_ROW1, PIN_ROW2, PIN_ROW3, PIN_ROW4};
static const int kCols[4] = {PIN_COL1, PIN_COL2, PIN_COL3, PIN_COL4};
static const char kMap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

void keypad_init(void) {
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(kRows[i]);
        gpio_set_direction(kRows[i], GPIO_MODE_OUTPUT_OD);  // linha "solta" = 1, ativa = 0
        gpio_set_level(kRows[i], 1);
        gpio_reset_pin(kCols[i]);
        gpio_set_direction(kCols[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(kCols[i], GPIO_PULLUP_ONLY);
    }
}

static char scan(void) {
    for (int r = 0; r < 4; r++) {
        gpio_set_level(kRows[r], 0);
        esp_rom_delay_us(5);
        for (int c = 0; c < 4; c++) {
            if (gpio_get_level(kCols[c]) == 0) {
                gpio_set_level(kRows[r], 1);
                return kMap[r][c];
            }
        }
        gpio_set_level(kRows[r], 1);
    }
    return 0;
}

char keypad_poll(void) {
    static char last = 0;
    static int stable = 0;
    char k = scan();
    if (k == last) {
        if (stable < 3) stable++;
        if (stable == 2 && k) return k;  // 2 leituras iguais seguidas (~40 ms) = toque válido
    } else {
        last = k;
        stable = 0;
    }
    return 0;
}
