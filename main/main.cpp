#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "Alarm.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

static const char *TAG = "Main";

bool initialize_flash()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_INVALID_STATE)
    {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
        return false;
    return true;
}

bool initialize_wifi()
{
    WiFiManager wifiManager;
    wifiManager.init();
    while (!wifiManager.is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return true;
}

extern "C" void app_main(void)
{
    // Setup
    LEDStrip strip(17, 80, false);
    initialize_flash();
    initialize_wifi();
    Alarm::init();
    WebServer server(80);
    if (server.start() != ESP_OK)
        return;

    ESP_LOGI(TAG, "Setup finished!");

    // Loop
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        SunriseSettings settings = server.get_settings_copy();

        ESP_LOGI(TAG, "Sunrise settings loaded: R=%d G=%d B=%d Light Preview: %s | Duration: %d min | On brightest: %d min | Alarm: %02d:%02d | Enabled: %s",
                 settings.red, settings.green, settings.blue, settings.light_preview ? "YES" : "NO", settings.duration_minutes,
                 settings.duration_on_brightest, settings.alarm_hour, settings.alarm_minute, settings.enabled ? "YES" : "NO");

        double sunrise_percentage;
        if (Alarm::is_alarm_time(settings, sunrise_percentage))
        {
            int red = static_cast<int>(settings.red * sunrise_percentage);
            int green = static_cast<int>(settings.green * sunrise_percentage);
            int blue = static_cast<int>(settings.blue * sunrise_percentage);

            red = std::min(255, std::max(0, red));
            green = std::min(255, std::max(0, green));
            blue = std::min(255, std::max(0, blue));

            for (int i = 0; i < 80; i++)
            {
                strip.setPixel(i, red, green, blue);
            }
            strip.refresh();
        }
        else if (settings.light_preview)
        {
            for (int i = 0; i < 80; i++)
            {
                strip.setPixel(i, settings.red, settings.green, settings.blue);
            }
            strip.refresh();
        }
        else
        {
            strip.clear();
            strip.refresh();
        }
    }
}
