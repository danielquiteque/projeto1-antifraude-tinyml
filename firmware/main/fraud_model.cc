#include "fraud_model.h"

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "fraud_model";

namespace {
const tflite::Model *g_model = nullptr;
tflite::MicroInterpreter *g_interp = nullptr;
TfLiteTensor *g_in = nullptr;
TfLiteTensor *g_out = nullptr;
// Tensor arena: RAM estática para ativações + estruturas do interpretador (sem malloc).
// Tamanho medido no notebook com o runtime TFLM + 50% de folga.
alignas(16) uint8_t g_arena[FRAUD_TENSOR_ARENA];
}  // namespace

extern "C" int fraud_model_init(void) {
    g_model = tflite::GetModel(g_model_data);
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "schema %lu != %d", (unsigned long)g_model->version(), TFLITE_SCHEMA_VERSION);
        return -1;
    }
    // Só registramos os operadores que o modelo usa (economiza Flash):
    // FULLY_CONNECTED (com ReLU fundida) e LOGISTIC (sigmoid).
    static tflite::MicroMutableOpResolver<2> resolver;
    resolver.AddFullyConnected();
    resolver.AddLogistic();

    static tflite::MicroInterpreter interp(g_model, resolver, g_arena, sizeof(g_arena));
    g_interp = &interp;
    if (g_interp->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors falhou — aumente FRAUD_TENSOR_ARENA");
        return -2;
    }
    g_in = g_interp->input(0);
    g_out = g_interp->output(0);
    ESP_LOGI(TAG, "modelo %u bytes | arena usada %u de %u bytes | entrada int8 scale=%.6f zp=%d | saída scale=%.6f zp=%d",
             g_model_data_len, (unsigned)g_interp->arena_used_bytes(), (unsigned)sizeof(g_arena), g_in->params.scale,
             (int)g_in->params.zero_point, g_out->params.scale, (int)g_out->params.zero_point);
    if (fabsf(g_in->params.scale - FRAUD_IN_SCALE) > 1e-6f || g_in->params.zero_point != FRAUD_IN_ZERO_POINT) {
        ESP_LOGW(TAG, "parâmetros de quantização diferentes do fraud_params.h — rode o notebook de novo");
    }
    return 0;
}

extern "C" void fraud_model_run(const float x[FRAUD_N_FEATURES], fraud_result_t *r) {
    // Quantização da entrada: q = round(x / scale) + zero_point, saturado em [-128, 127]
    const float s = g_in->params.scale;
    const int zp = g_in->params.zero_point;
    for (int i = 0; i < FRAUD_N_FEATURES; i++) {
        int q = (int)lroundf(x[i] / s) + zp;
        g_in->data.int8[i] = (int8_t)(q < -128 ? -128 : (q > 127 ? 127 : q));
    }
    int64_t t0 = esp_timer_get_time();
    TfLiteStatus st = g_interp->Invoke();
    r->t_inf_us = esp_timer_get_time() - t0;
    if (st != kTfLiteOk) ESP_LOGE(TAG, "Invoke falhou");
    r->q = g_out->data.int8[0];
    r->prob = (r->q - g_out->params.zero_point) * g_out->params.scale;
    r->alert = r->q >= FRAUD_THRESHOLD_Q;  // comparação direta no domínio int8
}

extern "C" size_t fraud_model_arena_used(void) { return g_interp ? g_interp->arena_used_bytes() : 0; }
extern "C" size_t fraud_model_size(void) { return g_model_data_len; }
