// Inferência do MLP int8 com TensorFlow Lite Micro.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "fraud_params.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t q;          // saída quantizada (int8)
    float prob;        // saída desquantizada: (q - zero_point) * scale
    int alert;         // 1 = bloquear
    int64_t t_inf_us;  // tempo do Invoke()
} fraud_result_t;

int fraud_model_init(void);  // 0 = ok
void fraud_model_run(const float x[FRAUD_N_FEATURES], fraud_result_t *r);
size_t fraud_model_arena_used(void);
size_t fraud_model_size(void);

#ifdef __cplusplus
}
#endif
