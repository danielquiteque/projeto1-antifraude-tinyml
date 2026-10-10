// Projeto 1 — Terminal de pagamento com detecção de fraude embarcada (ESP32-S3 + TFLite Micro).
//
// Pipeline por transação (tudo no dispositivo):
//   teclado (valor, categoria) + RTC DS1307 (hora)  →  estado do cartão (Δt, EMA, janela 24 h)
//   →  22 features padronizadas  →  quantização int8  →  MLP int8 (TFLM)  →  APROVADA / BLOQUEADA
//
// Teclas:  0-9 valor (centavos)   * apaga   # confirma   C limpa
//          A próxima categoria    B adianta o RTC 1 h (testar compras de madrugada)
//          D liga/desliga o REPLAY de transações reais do conjunto de teste
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "ds1307.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fraud_features.h"
#include "fraud_model.h"
#include "fraud_params.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keypad.h"
#include "net.h"
#include "replay_data.h"
#include "ssd1306.h"

static const char *TAG = "antifraude";

// Rótulos curtos (cabem em 21 colunas do OLED), mesma ordem de kFraudCategoryKeys.
static const char *const kCatLabel[FRAUD_N_CATEGORIES] = {
    "entretenimento", "restaurante",  "combustivel",  "mercado web", "mercado loja",
    "saude/fitness",  "casa",         "criancas/pets", "diversos web", "diversos loja",
    "cuidado pessoal", "compras web", "compras loja", "viagem"};

static ssd1306_t s_oled;
static bool s_rtc_ok;

typedef enum { MODE_MANUAL, MODE_REPLAY } mode_t_;
static mode_t_ s_mode = MODE_MANUAL;

// --- estado do modo manual ---
static card_state_t s_demo_card;  // cartão de demonstração (titular de 35 anos)
static uint32_t s_cents = 0;
static int s_cat = 4;  // começa em "mercado loja"

// --- estado do modo replay ---
static card_state_t s_replay_cards[REPLAY_N_CARDS];
static uint32_t s_replay_clock[REPLAY_N_CARDS];
static int s_replay_i = 0;
static int s_hits = 0, s_frauds = 0, s_false_alarms = 0;

// --- última decisão (para a tela) ---
typedef struct {
    bool valid;
    float amt;
    int cat;
    int hour, minute;
    fraud_feat_t f;
    fraud_result_t r;
    int64_t t_feat_us;
    int label;  // -1 = desconhecido (manual)
} last_t;
static last_t s_last;

static void rtc_now(rtc_time_t *t) {
    if (!s_rtc_ok || ds1307_read(t) != ESP_OK) {  // fallback: relógio interno
        rtc_from_unix(1735689600u + (uint32_t)(esp_timer_get_time() / 1000000), t);
    }
}

// ----------------------------------------------------------------------------------------------
static void draw(void) {
    ssd1306_t *d = &s_oled;
    char buf[32];
    rtc_time_t now;
    rtc_now(&now);
    ssd1306_clear(d);

    // cabeçalho em vídeo reverso
    snprintf(buf, sizeof(buf), "%s %02d:%02d %s", s_mode == MODE_MANUAL ? "MANUAL" : "REPLAY", now.hour, now.minute,
             net_mqtt_ok() ? "MQTT" : (net_wifi_ok() ? "WiFi" : "off"));
    ssd1306_text(d, 1, 1, buf, 1);
    ssd1306_invert(d, 0, 0, 128, 9);

    if (s_mode == MODE_MANUAL) {
        snprintf(buf, sizeof(buf), "A> %s", kCatLabel[s_cat]);
        ssd1306_text(d, 0, 12, buf, 1);
        snprintf(buf, sizeof(buf), "$%lu.%02lu", (unsigned long)(s_cents / 100), (unsigned long)(s_cents % 100));
        ssd1306_text(d, 0, 22, buf, 2);
    } else {
        int card = s_replay_i > 0 ? kReplay[s_replay_i - 1].card : 0;
        snprintf(buf, sizeof(buf), "tx %d/%d cartao %d", s_replay_i, REPLAY_LEN, card);
        ssd1306_text(d, 0, 12, buf, 1);
        if (s_last.valid) {
            snprintf(buf, sizeof(buf), "%02d:%02d %s", s_last.hour, s_last.minute, kCatLabel[s_last.cat]);
            ssd1306_text(d, 0, 21, buf, 1);
            snprintf(buf, sizeof(buf), "$%.2f", s_last.amt);
            ssd1306_text(d, 0, 30, buf, 1);
            snprintf(buf, sizeof(buf), "acerto %d/%d", s_hits, s_frauds);
            ssd1306_text(d, 64, 30, buf, 1);
        }
    }

    ssd1306_hline(d, 0, 40, 128);
    if (s_last.valid) {
        // decisão + barra de score
        const char *dec = s_last.r.alert ? "BLOQUEADA" : "APROVADA";
        ssd1306_text(d, 0, 43, dec, 1);
        snprintf(buf, sizeof(buf), "%3d%%", (int)(s_last.r.prob * 100.0f + 0.5f));
        ssd1306_text(d, 100, 43, buf, 1);
        int w = (int)(s_last.r.prob * 40.0f);
        ssd1306_rect(d, 57, 43, 42, 7, false);
        ssd1306_rect(d, 58, 44, w, 5, true);
        if (s_last.r.alert) ssd1306_invert(d, 0, 42, 56, 9);
        if (s_last.label >= 0) {
            snprintf(buf, sizeof(buf), "real:%s", s_last.label ? "FRAUDE" : "legit");
        } else {
            snprintf(buf, sizeof(buf), "24h:%d dt:%.0fm", s_last.f.cnt24, (expf(s_last.f.raw[4]) - 1.0f) / 60.0f);
        }
        ssd1306_text(d, 0, 52, buf, 1);
        snprintf(buf, sizeof(buf), "%lluus", (unsigned long long)s_last.r.t_inf_us);
        ssd1306_text(d, 128 - 6 * (int)strlen(buf), 56, buf, 1);
    } else {
        ssd1306_text(d, 0, 44, "#=pagar A=categoria", 1);
        ssd1306_text(d, 0, 54, "B=+1h  D=replay", 1);
    }
    ssd1306_flush(d);
}

// ----------------------------------------------------------------------------------------------
static void decide(card_state_t *card, uint32_t ts, float amt, int cat, int hour, int minute, int label,
                   const char *src, int idx, int card_id) {
    s_last.valid = true;
    s_last.amt = amt;
    s_last.cat = cat;
    s_last.hour = hour;
    s_last.minute = minute;
    s_last.label = label;

    int64_t t0 = esp_timer_get_time();
    fraud_features(card, ts, amt, cat, hour, minute, &s_last.f);
    s_last.t_feat_us = esp_timer_get_time() - t0;
    fraud_model_run(s_last.f.x, &s_last.r);

    // Atuadores: LED + bip
    board_leds(!s_last.r.alert, s_last.r.alert);

    // Log serial em formato CSV — compare com artefatos_antifraude/replay_esperado.csv do notebook
    printf("TX,%s,%d,%d,%.2f,%s,%02d:%02d,%d,%.3f,%d,%d,%lld,%lld\n", src, idx, card_id, amt, kFraudCategoryKeys[cat],
           hour, minute, s_last.r.q, s_last.r.prob, s_last.r.alert, label, (long long)s_last.t_feat_us,
           (long long)s_last.r.t_inf_us);

    char json[384];
    snprintf(json, sizeof(json),
             "{\"src\":\"%s\",\"i\":%d,\"card\":%d,\"amt\":%.2f,\"cat\":\"%s\",\"hora\":\"%02d:%02d\",\"q\":%d,"
             "\"p\":%.4f,\"alerta\":%d,\"real\":%d,\"cnt24\":%d,\"sum24\":%.2f,\"dt_s\":%.0f,\"amt_vs_ema\":%.3f,"
             "\"t_feat_us\":%lld,\"t_inf_us\":%lld}",
             src, idx, card_id, amt, kFraudCategoryKeys[cat], hour, minute, s_last.r.q, s_last.r.prob, s_last.r.alert,
             label, s_last.f.cnt24, s_last.f.sum24, expf(s_last.f.raw[4]) - 1.0f, s_last.f.raw[5],
             (long long)s_last.t_feat_us, (long long)s_last.r.t_inf_us);
    net_publish("antifraude/tx", json);

    draw();
    if (s_last.r.alert) {
        board_beep(1800, 120);
        vTaskDelay(pdMS_TO_TICKS(60));
        board_beep(1800, 120);
    } else {
        board_beep(2600, 60);
    }
}

static void manual_pay(void) {
    if (s_cents == 0) return;
    rtc_time_t t;
    rtc_now(&t);
    decide(&s_demo_card, rtc_to_unix(&t), s_cents / 100.0f, s_cat, t.hour, t.minute, -1, "manual", 0, -1);
    s_cents = 0;
}

static void replay_reset(void) {
    for (int i = 0; i < REPLAY_N_CARDS; i++) {
        card_init(&s_replay_cards[i], kReplayCardAge[i]);
        s_replay_clock[i] = 1593561600u;  // relógio virtual do replay (só as diferenças Δt importam)
    }
    s_replay_i = 0;
    s_hits = s_frauds = s_false_alarms = 0;
}

static void replay_step(void) {
    if (s_replay_i >= REPLAY_LEN) {
        ESP_LOGI(TAG, "replay fim: %d/%d fraudes detectadas, %d falsos alarmes", s_hits, s_frauds, s_false_alarms);
        replay_reset();
    }
    const replay_tx_t *tx = &kReplay[s_replay_i];
    s_replay_clock[tx->card] += tx->dt_s;
    decide(&s_replay_cards[tx->card], s_replay_clock[tx->card], tx->amount, tx->category, tx->hour, tx->minute,
           tx->label, "replay", s_replay_i, tx->card);
    if (tx->label) {
        s_frauds++;
        s_hits += s_last.r.alert;
    } else {
        s_false_alarms += s_last.r.alert;
    }
    s_replay_i++;
    draw();
}

static void rtc_add_hour(void) {
    if (!s_rtc_ok) return;
    rtc_time_t t;
    if (ds1307_read(&t) != ESP_OK) return;
    rtc_from_unix(rtc_to_unix(&t) + 3600u, &t);
    ds1307_write(&t);
}

// ----------------------------------------------------------------------------------------------
extern "C" void app_main(void) {
    board_io_init();
    keypad_init();
    i2c_master_bus_handle_t bus = board_i2c_init();
    ESP_ERROR_CHECK(ssd1306_init(&s_oled, bus, OLED_ADDR));
    s_rtc_ok = ds1307_init(bus, DS1307_ADDR) == ESP_OK;
    if (!s_rtc_ok) ESP_LOGW(TAG, "DS1307 não encontrado — usando relógio interno");

    ssd1306_clear(&s_oled);
    ssd1306_text(&s_oled, 0, 0, "Antifraude TinyML", 1);
    ssd1306_text(&s_oled, 0, 12, "carregando modelo...", 1);
    ssd1306_flush(&s_oled);

    if (fraud_model_init() != 0) {
        ssd1306_text(&s_oled, 0, 30, "ERRO no modelo", 1);
        ssd1306_flush(&s_oled);
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "modelo %u B int8", (unsigned)fraud_model_size());
    ssd1306_text(&s_oled, 0, 24, buf, 1);
    snprintf(buf, sizeof(buf), "arena %u B", (unsigned)fraud_model_arena_used());
    ssd1306_text(&s_oled, 0, 34, buf, 1);
    ssd1306_text(&s_oled, 0, 46, "conectando Wi-Fi...", 1);
    ssd1306_flush(&s_oled);

    net_start(8000);  // opcional: se falhar, segue offline
    snprintf(buf, sizeof(buf), "{\"evento\":\"boot\",\"modelo_bytes\":%u,\"arena\":%u,\"threshold\":%.4f}",
             (unsigned)fraud_model_size(), (unsigned)fraud_model_arena_used(), FRAUD_THRESHOLD);
    net_publish("antifraude/status", buf);

    card_init(&s_demo_card, 35.0f);
    replay_reset();
    printf("TX,src,i,card,amt,cat,hora,y_q,p,alerta,real,t_feat_us,t_inf_us\n");
    draw();

    int64_t last_replay = 0, last_draw = 0;
    for (;;) {
        char k = keypad_poll();
        if (k) {
            if (k >= '0' && k <= '9' && s_mode == MODE_MANUAL) {
                if (s_cents < 10000000) s_cents = s_cents * 10 + (uint32_t)(k - '0');
            } else if (k == '*') {
                s_cents /= 10;
            } else if (k == 'C') {
                s_cents = 0;
                s_last.valid = false;
                board_leds(false, false);
            } else if (k == '#' && s_mode == MODE_MANUAL) {
                manual_pay();
            } else if (k == 'A') {
                s_cat = (s_cat + 1) % FRAUD_N_CATEGORIES;
            } else if (k == 'B') {
                rtc_add_hour();
            } else if (k == 'D') {
                s_mode = s_mode == MODE_MANUAL ? MODE_REPLAY : MODE_MANUAL;
                s_last.valid = false;
                board_leds(false, false);
            }
            draw();
        }
        int64_t now = esp_timer_get_time();
        if (s_mode == MODE_REPLAY && now - last_replay > 1500000) {  // 1 transação a cada 1,5 s
            last_replay = now;
            replay_step();
        }
        if (now - last_draw > 1000000) {  // relógio na tela
            last_draw = now;
            draw();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
