// Varredura de teclado matricial 4x4 com debounce por software.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void keypad_init(void);
// Chamar periodicamente (~20 ms). Retorna o caractere de uma tecla recém-pressionada ou 0.
char keypad_poll(void);

#ifdef __cplusplus
}
#endif
