#pragma once
// Gerado pelo notebook 01_antifraude_treino_compressao.ipynb — NÃO editar à mão.
// Modelo: qat | 4168 bytes | threshold escolhido na validação com recall >= 90%
#include <stdint.h>

#define FRAUD_N_FEATURES      22
#define FRAUD_N_CONT          8
#define FRAUD_N_CATEGORIES    14
#define FRAUD_DT_DEFAULT_S    604800u
#define FRAUD_EMA_ALPHA       0.1f
#define FRAUD_WIN_24H_S       86400u
#define FRAUD_TENSOR_ARENA    3072   // medido no TFLM: 1600 bytes (+50% de folga)

#define FRAUD_THRESHOLD       0.074219f
#define FRAUD_THRESHOLD_Q     (-109)
#define FRAUD_IN_SCALE        0.0422093123f
#define FRAUD_IN_ZERO_POINT   (-12)
#define FRAUD_OUT_SCALE       0.0039062500f
#define FRAUD_OUT_ZERO_POINT  (-128)

// ordem: log_amt, hour_sin, hour_cos, age, log_dt, amt_vs_ema, log_cnt24, log_sum24
__attribute__((unused)) static const float kFraudMu[FRAUD_N_CONT]    = { 3.51676130f, -0.14409681f, 0.00362011f, 38.40279388f, 9.19230270f, -0.65639210f, 1.43542671f, 4.76640797f };
__attribute__((unused)) static const float kFraudSigma[FRAUD_N_CONT] = { 1.31300759f, 0.69166780f, 0.70768601f, 18.08572960f, 1.74085200f, 1.31679940f, 0.66937393f, 1.83658636f };

__attribute__((unused)) static const char *const kFraudCategoryKeys[FRAUD_N_CATEGORIES] = { "entertainment", "food_dining", "gas_transport", "grocery_net", "grocery_pos", "health_fitness", "home", "kids_pets", "misc_net", "misc_pos", "personal_care", "shopping_net", "shopping_pos", "travel" };
