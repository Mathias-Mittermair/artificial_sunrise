#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "WebServer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "Main";

extern "C" void app_main(void) {
    // NVS für WiFi Init
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // WLAN Station
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, "Box1Benninger");
    strcpy((char*)wifi_config.sta.password, "benninger29");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "WLAN gestartet, warte auf Verbindung...");

    // kleinen Delay geben bis WiFi connected ist
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Webserver starten
    WebServer server(80);
    server.start();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }


    // LEDStrip strip(17, 80, false);

    // while (true) {
    //     for (int i = 0; i < 80; i++) {
    //         strip.setPixel(i, 20, 0, 0);
    //     }
    //     strip.refresh();
    //     ESP_LOGI(TAG, "LEDs ON");

    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     strip.clear();
    //     ESP_LOGI(TAG, "LEDs OFF");

    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
