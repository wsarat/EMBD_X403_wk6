#include "wifi.h"
#include "storage.h"

static const char *WIFI_TAG = "wifi";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAXIMUM_RETRY  3

#define DEFAULT_SCAN_LIST_SIZE 10

static bool wifi_sta_mode = 1;

void get_device_name(char *device_name) {
    esp_err_t ret = ESP_FAIL;
    const uint8_t mac[6];    

    ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret) {
        ESP_LOGE(WIFI_TAG, "esp_read_mac error!");
        sprintf(device_name, "esp32-unknown");   
    }
    
    sprintf(device_name, "esp32-%x%x", mac[4], mac[5]);    

    return;
}

void start_mdns_service() {
    esp_err_t ret = ESP_FAIL;
    char mdns_name[16];
    get_device_name(mdns_name);

    ret = mdns_init();
    if (ret) 
        ESP_LOGE(WIFI_TAG, "mdns_init error!");

    ret = mdns_hostname_set(mdns_name);
    if (ret) 
        ESP_LOGE(WIFI_TAG, "mdns_hostname_set error!");

    ESP_LOGI(WIFI_TAG, "mDNS name: %s", mdns_name);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (wifi_sta_mode)
            esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_sta_mode) {
            if (s_retry_num < WIFI_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(WIFI_TAG,"connect to the AP fail");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else {
        ESP_LOGI(WIFI_TAG, "got event_id: %ld  unhandled", event_id);
    }
}

void _netif_init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();   
    //esp_netif_create_default_wifi_ap();
}

// void wifi_ftm_initiate(struct my_mac_t *mac_addr, uint8_t channel) {
//     wifi_ftm_initiator_cfg_t ftm_cfg = {
//         .frm_count = 32,
//         .burst_period = 2,
//     };
//     memcpy(ftm_cfg.resp_mac, mac_addr, sizeof(ftm_cfg.resp_mac));
//     ftm_cfg.channel = channel;

//     if (ESP_OK != esp_wifi_ftm_initiate_session(&ftm_cfg)) {
//         ESP_LOGE(WIFI_TAG, "Failed to start FTM session");
//         return;
//     }    
//     if (ESP_OK != esp_wifi_ftm_initiate_session(&ftmi_cfg)) {
//         ESP_LOGE(WIFI_TAG, "Failed to start FTM session");
//         return 0;
//     }

//     bits = xEventGroupWaitBits(s_ftm_event_group, FTM_REPORT_BIT | FTM_FAILURE_BIT,
//                                            pdTRUE, pdFALSE, portMAX_DELAY);
//     /* Processing data from FTM session */
//     ftm_process_report();
//     free(s_ftm_report);
//     s_ftm_report = NULL;
//     s_ftm_report_num_entries = 0;
//     if (bits & FTM_REPORT_BIT) {
//         ESP_LOGI(WIFI_TAG, "Estimated RTT - %" PRId32 " nSec, Estimated Distance - %" PRId32 ".%02" PRId32 " meters",
//                           s_rtt_est, s_dist_est / 100, s_dist_est % 100);
//     } else {
//         /* Failure case */
//     }

//     return 0;    
// }

void wifi_ftm_responder(uint8_t channel) {
    wifi_config_t ap_config = {
        .ap.max_connection = 4,
        .ap.authmode = WIFI_AUTH_WPA2_PSK,
        .ap.ftm_responder = true
    };
    
    strlcpy((char*) ap_config.ap.ssid, "ESP32-S3", MAX_SSID_LEN);
    strlcpy((char*) ap_config.ap.password, "12345678", MAX_PASSPHRASE_LEN);

    if (strlen((char *)ap_config.ap.password) == 0)
        ap_config.ap.authmode = WIFI_AUTH_OPEN;

    ap_config.ap.channel = channel;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    
    int bw = 40;
    esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, (bw == 40)? WIFI_BW_HT40:WIFI_BW_HT20);
    esp_wifi_ftm_resp_set_offset(0); //Set offset in cm for FTM Responder.
    ap_config.ap.ftm_responder = true; // already done in wifi_config_t
    esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
    ESP_LOGI(WIFI_TAG, "Starting SoftAP with FTM Responder support, SSID - %s, Password - %s, Primary Channel - %d, Bandwidth - %dMHz",
                        ap_config.ap.ssid, ap_config.ap.password, channel, bw);    
    return;  
}

void wifi_scan(wifi_ap_record_t *ap_info, uint16_t *max) {
    // uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    // wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    // memset(ap_info, 0, sizeof(ap_info));
    uint16_t ap_count = 0;

    // wifi_mode_t current_wifi_mode;
    // esp_wifi_get_mode(&current_wifi_mode);
    // if (current_wifi_mode == WIFI_MODE_AP)
    //     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    //     ESP_ERROR_CHECK(esp_wifi_start() );

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 10,
        .scan_time.active.max = 100        
    };

    esp_wifi_scan_start(&scan_config, true /*block*/);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(max, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(WIFI_TAG, "Total APs scanned = %u, number = %u", ap_count, *max);
    // for (int i = 0; i < *max; i++) {
    //     ESP_LOGI(WIFI_TAG, "SSID \t\t%s", *ap_info[i]->ssid);
    //     ESP_LOGI(WIFI_TAG, "RSSI \t\t%d", *ap_info[i]->rssi);
    //     // print_auth_mode(ap_info[i].authmode);
    //     // if (ap_info[i].authmode != WIFI_AUTH_WEP) {
    //     //     print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
    //     // }
    //     ESP_LOGI(WIFI_TAG, "Channel \t\t%d", *ap_info[i]->primary);
    // }    
    //  if (current_wifi_mode == WIFI_MODE_AP)
    //     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
}

esp_err_t wifi_connect(const char* ssid, const char* password) {   
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_err_t ret = ESP_FAIL;

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            //.ssid = "CefiroIOT",
            //.password = "CefiroA32",
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected");

        start_mdns_service();
        httpServer_start();

        ret = ESP_OK;

    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect.");
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);   

    return ret;
}

void wifi_init_ap() {
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    char device_name[32];
    get_device_name(device_name);

    wifi_config_t wifi_config = {
        .ap = {
            //.ssid = "",
            .ssid_len = strlen(device_name),
            .channel = 1,
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    strncpy(&wifi_config.ap.ssid, device_name, sizeof(device_name));

    if (strlen((char *)wifi_config.ap.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA/*WIFI_MODE_AP*/));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             device_name, wifi_config.ap.password, wifi_config.ap.channel);

    start_mdns_service();
    httpServer_start();
}

void wifi_init() {
    _netif_init();

    esp_err_t ret = ESP_FAIL;
    char ssid[16];
    char password[32];

    if ( nvs_get(KEY_SSID, ssid) == ESP_OK 
        && nvs_get(KEY_PASSWD, password) == ESP_OK ) {

        ESP_LOGI(WIFI_TAG, "SSID: %s, Password: %s", ssid, password);
        
        if (strlen(ssid) == 0 || strlen(password) == 0 ) {
            ESP_ERROR_CHECK(esp_wifi_stop());
            wifi_sta_mode = 0; // AP mode
        }
        
    } else {
        wifi_sta_mode = 0; // AP mode
    }

    if (wifi_sta_mode) {
        wifi_init_config_t wifi_conf = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifi_conf));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) );
        ESP_ERROR_CHECK(esp_wifi_start() );

        ret = wifi_connect(ssid, password);
        if (ret != ESP_OK)
            wifi_init_ap();

    } else {
        // AP mode
        wifi_init_ap();
    }
}

