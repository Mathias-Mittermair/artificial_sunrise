// WiFiManager.cpp
#include "WiFiManager.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "secrets.h"

static const char *TAG = "WiFiManager";
static bool s_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi started, connecting to AP...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t* disc = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Disconnected from AP! Reason: %d", disc->reason);
        esp_wifi_connect();
        s_connected = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "✅ Got IP address");
        s_connected = true;
    }
}

void WiFiManager::init()
{
    // Initialize TCP/IP network interface (required)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Register event handlers BEFORE starting Wi-Fi
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set mode first
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Configure Wi-Fi credentials
    wifi_config_t wifi_config = {0}; // Zero-initialize!
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);

    // 🔑 Critical: Set auth mode to match your router
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    // Optional: improve WPA3 compatibility
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    // ⚠️ Important: Set scan method for better compatibility
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start Wi-Fi (triggers WIFI_EVENT_STA_START)
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool WiFiManager::is_connected()
{
    return s_connected;
}