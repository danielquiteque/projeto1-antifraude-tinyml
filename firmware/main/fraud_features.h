// Engenharia de features NO DISPOSITIVO — espelho exato de build_features()/device_features() do notebook.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "fraud_params.h"

#define FRAUD_HIST_LEN 48  // > máximo de compras em 24 h observado no dataset (37)

typedef struct {
    uint32_t ts;
    float amt;
} fraud_hist_t;

// Estado por cartão (~400 bytes de RAM).
typedef struct {
    float age;
    bool has_prev;
    uint32_t last_ts;
    float ema;  // média móvel exponencial dos valores
    fraud_hist_t hist[FRAUD_HIST_LEN];
    int head;   // próxima posição de escrita (buffer circular)
    int count;  // itens válidos
} card_state_t;

typedef struct {
    float x[FRAUD_N_FEATURES];  // vetor já padronizado, pronto para quantizar
    float raw[FRAUD_N_CONT];    // features antes da padronização (para log/dashboard)
    int cnt24;
    float sum24;
} fraud_feat_t;

#ifdef __cplusplus
extern "C" {
#endif

void card_init(card_state_t *c, float age);
// Calcula as features da transação (ts, valor, categoria, hora local) e ATUALIZA o estado do cartão.
void fraud_features(card_state_t *c, uint32_t ts, float amt, int category, int hour, int minute, fraud_feat_t *out);

#ifdef __cplusplus
}
#endif
