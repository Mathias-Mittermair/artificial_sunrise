#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

static const char *TAG = "Main";

extern "C" void app_main(void)
{
    // === STEP 1: Initialize NVS ===
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_INVALID_STATE)
    {
        ESP_LOGW(TAG, "NVS version mismatch or corruption. Erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    // === STEP 2: Now initialize Wi-Fi (which uses NVS) ===
    WiFiManager wifiManager;
    wifiManager.init(); // This will now succeed

    // === STEP 3: Wait for connection ===
    while (!wifiManager.is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "Waiting for WiFi...");
    }

    ESP_LOGI(TAG, "Wi-Fi connected. Starting web server...");
    WebServer server(80);
    esp_err_t err = server.start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(err));
    }

    while (true)
    {
        const auto &settings = server.get_settings();

        if (settings.enabled)
        {
            ESP_LOGI("Main", "Sunrise active: R=%d G=%d B=%d, Brightness=%d, Duration=%d min",
                     settings.red, settings.green, settings.blue,
                     settings.brightness, settings.duration_minutes);

            // Your LED logic here:
            // LEDStrip strip(17, 80, false);
            // strip.setAll(settings.red, settings.green, settings.blue);
            // etc.
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 sec
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
