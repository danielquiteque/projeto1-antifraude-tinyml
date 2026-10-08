// Conectividade opcional: Wi-Fi (Wokwi-GUEST) + MQTT para o dashboard.
// A inferência NÃO depende da rede: se o Wi-Fi falhar, o dispositivo continua decidindo localmente.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Conecta ao Wi-Fi e inicia o cliente MQTT. Bloqueia até `timeout_ms`. Retorna true se obteve IP.
bool net_start(int timeout_ms);
bool net_wifi_ok(void);
bool net_mqtt_ok(void);
// Publica `json` em "<CONFIG_NET_TOPIC_PREFIX>/<suffix>" (QoS 0). Ignora silenciosamente se offline.
void net_publish(const char *suffix, const char *json);

#ifdef __cplusplus
}
#endif
