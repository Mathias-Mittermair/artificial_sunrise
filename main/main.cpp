#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "Alarm.h"
#include "Settings.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

static const char *TAG = "Main";

void switch_init(const LowLevelSettings& settings)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    io_conf.pin_bit_mask = 1ULL << settings.pin_alarm_switch; // Beispiel Pin
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = 1ULL << settings.pin_light_switch;
    gpio_config(&io_conf);
}

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
     Settings::get().init();
    LowLevelSettings& low_level_settings = Settings::get().getSettings();

    LEDStrip strip(low_level_settings.pin_led, low_level_settings.num_leds, false);

    initialize_flash();
    initialize_wifi();
    Alarm::init();
    WebServer server(low_level_settings.port);
    if (server.start() != ESP_OK)
        return;

    switch_init(low_level_settings);
    ESP_LOGI(TAG, "Setup finished!");

    // Loop
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(low_level_settings.cycle_sleep));

        int level_alarm = gpio_get_level(low_level_settings.pin_alarm_switch);
        int level_light_preview = gpio_get_level(low_level_settings.pin_light_switch);
        ESP_LOGI(TAG, "level_alarm is %s and level_light_preview is %s", level_alarm ? "ON" : "OFF", level_light_preview ? "ON" : "OFF");

        server.set_alarm_enabled(level_alarm == 1);
        server.set_light_preview(level_light_preview == 1);

        SunriseSettings settings = server.get_settings_copy();

        ESP_LOGI(TAG, "Sunrise settings loaded: R=%d G=%d B=%d Light Preview: %s | Duration: %d min | On brightest: %d min | Alarm: %02d:%02d | Enabled: %s",
                 settings.red, settings.green, settings.blue, settings.light_preview ? "YES" : "NO", settings.duration_minutes,
                 settings.duration_on_brightest, settings.alarm_hour, settings.alarm_minute, settings.alarm_enabled ? "YES" : "NO");

        double sunrise_percentage;
        if (Alarm::is_alarm_time(settings, sunrise_percentage))
        {
            int red = static_cast<int>(low_level_settings.sunrise_red * sunrise_percentage);
            int green = static_cast<int>(low_level_settings.sunrise_green * sunrise_percentage);
            int blue = static_cast<int>(low_level_settings.sunrise_blue * sunrise_percentage);

            red = std::min(255, std::max(0, red));
            green = std::min(255, std::max(0, green));
            blue = std::min(255, std::max(0, blue));

            for (int i = 0; i < low_level_settings.num_leds; i++)
            {
                strip.setPixel(i, red, green, blue);
            }
            strip.refresh();
        }
        else if (settings.light_preview)
        {
            for (int i = 0; i < low_level_settings.num_leds; i++)
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
