#include "net.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "net";
static EventGroupHandle_t s_ev;
#define BIT_IP BIT0
static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_mqtt_ok;

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_ev, BIT_IP);
        esp_wifi_connect();  // reconecta sempre (conectividade intermitente)
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_ev, BIT_IP);
    }
}

static void on_mqtt(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == MQTT_EVENT_CONNECTED) {
        s_mqtt_ok = true;
        ESP_LOGI(TAG, "MQTT conectado em %s", CONFIG_NET_MQTT_URI);
    } else if (id == MQTT_EVENT_DISCONNECTED) {
        s_mqtt_ok = false;
    }
}

bool net_start(int timeout_ms) {
#if !CONFIG_NET_ENABLE
    return false;
#else
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    s_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, CONFIG_NET_WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, CONFIG_NET_WIFI_PASSWORD, sizeof(wc.sta.password) - 1);
    wc.sta.channel = CONFIG_NET_WIFI_CHANNEL;  // Wokwi-GUEST usa o canal 6: conecta bem mais rápido
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t b = xEventGroupWaitBits(s_ev, BIT_IP, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    if (!(b & BIT_IP)) {
        ESP_LOGW(TAG, "sem Wi-Fi após %d ms — seguindo offline (inferência continua local)", timeout_ms);
        return false;
    }
    esp_mqtt_client_config_t mc = {.broker.address.uri = CONFIG_NET_MQTT_URI};
    s_mqtt = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, on_mqtt, NULL);
    esp_mqtt_client_start(s_mqtt);
    return true;
#endif
}

bool net_wifi_ok(void) { return s_ev && (xEventGroupGetBits(s_ev) & BIT_IP); }
bool net_mqtt_ok(void) { return s_mqtt_ok; }

void net_publish(const char *suffix, const char *json) {
    if (!s_mqtt || !s_mqtt_ok) return;
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s", CONFIG_NET_TOPIC_PREFIX, suffix);
    esp_mqtt_client_publish(s_mqtt, topic, json, 0, 0, 0);
}
