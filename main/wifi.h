#ifndef HEADER_WIFI_H
#define HEADER_WIFI_H

#include "esp_log.h"

#include "esp_wifi.h"
#include "esp_event.h"  //esp_event_loop_create_default
#include "freertos/event_groups.h"  //xEventGroupSetBits

#include "lwip/err.h"   // netif
#include "lwip/sys.h"

#include "esp_mac.h"
#include "mdns.h"

#include "httpServer.h"


void wifi_init();
void wifi_scan(wifi_ap_record_t *ap_info, uint16_t *max);
esp_err_t wifi_connect(const char* ssid, const char* password);

#endif