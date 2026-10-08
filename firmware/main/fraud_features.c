#include "fraud_features.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void card_init(card_state_t *c, float age) {
    memset(c, 0, sizeof(*c));
    c->age = age;
}

void fraud_features(card_state_t *c, uint32_t ts, float amt, int category, int hour, int minute, fraud_feat_t *out) {
    // 1) janela das últimas 24 h: compras anteriores com ts_prev >= ts - 24h
    int cnt = 0;
    float sum = 0.0f;
    for (int i = 0; i < c->count; i++) {
        const fraud_hist_t *h = &c->hist[i];
        if ((int64_t)h->ts >= (int64_t)ts - FRAUD_WIN_24H_S) {
            cnt++;
            sum += h->amt;
        }
    }
    // 2) features contínuas (mesma ordem de CONT_FEATURES no notebook)
    float hh = (float)hour + (float)minute / 60.0f;
    int64_t dt = c->has_prev ? (int64_t)ts - (int64_t)c->last_ts : (int64_t)FRAUD_DT_DEFAULT_S;
    if (dt < 1) dt = 1;
    float *r = out->raw;
    r[0] = log1pf(amt);
    r[1] = sinf(2.0f * (float)M_PI * hh / 24.0f);
    r[2] = cosf(2.0f * (float)M_PI * hh / 24.0f);
    r[3] = c->age;
    r[4] = log1pf((float)dt);
    r[5] = c->has_prev ? log1pf(amt) - log1pf(c->ema) : 0.0f;
    r[6] = log1pf((float)cnt);
    r[7] = log1pf(sum);

    // 3) padronização (μ, σ do treino) + one-hot da categoria
    for (int i = 0; i < FRAUD_N_CONT; i++) out->x[i] = (r[i] - kFraudMu[i]) / kFraudSigma[i];
    for (int i = 0; i < FRAUD_N_CATEGORIES; i++) out->x[FRAUD_N_CONT + i] = (i == category) ? 1.0f : 0.0f;
    out->cnt24 = cnt;
    out->sum24 = sum;

    // 4) atualiza o estado do cartão
    c->ema = c->has_prev ? (1.0f - FRAUD_EMA_ALPHA) * c->ema + FRAUD_EMA_ALPHA * amt : amt;
    c->has_prev = true;
    c->last_ts = ts;
    // descarta o que saiu da janela de 24 h (compacta o buffer) e insere a compra atual
    int k = 0;
    for (int i = 0; i < c->count; i++)
        if ((int64_t)c->hist[i].ts >= (int64_t)ts - FRAUD_WIN_24H_S) c->hist[k++] = c->hist[i];
    c->count = k;
    if (c->count == FRAUD_HIST_LEN) {  // cheio: remove o mais antigo
        memmove(&c->hist[0], &c->hist[1], sizeof(fraud_hist_t) * (FRAUD_HIST_LEN - 1));
        c->count--;
    }
    c->hist[c->count].ts = ts;
    c->hist[c->count].amt = amt;
    c->count++;
}
