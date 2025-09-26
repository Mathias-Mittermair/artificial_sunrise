#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include <time.h>
#include "esp_sntp.h"

static const char *TAG = "Main";

// Add this function to initialize SNTP
void initialize_sntp()
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org"); // or "time.google.com", etc.
    sntp_init();
}

void set_timezone(const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();
}

// Optional: helper to wait until time is synced
bool obtain_time(int timeout_sec = 30)
{
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = timeout_sec;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    return timeinfo.tm_year >= (2023 - 1900); // basic sanity check
}

bool is_alarm_time(const SunriseSettings &settings, double &sunrise_percentage)
{
    if (!settings.enabled)
    {
        ESP_LOGI(TAG, "Alarm disabled!");
        return false;
    }

    time_t now = time(nullptr);
    struct tm local_time;
    localtime_r(&now, &local_time);

    ESP_LOGI(TAG, "Current Time = %02d:%02d:%02d",
             local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

    // Convert everything to minutes since midnight
    int current_total_minutes = local_time.tm_hour * 60 + local_time.tm_min;
    int alarm_start_minutes = settings.alarm_hour * 60 + settings.alarm_minute;
    int alarm_end_minutes = alarm_start_minutes + settings.duration_minutes;

    // Handle wrap-around midnight (optional, but good practice)
    // For simplicity, assume alarm doesn't cross midnight (common for sunrise)
    if (alarm_end_minutes > 24 * 60)
    {
        // Not handled here — skip if you don't need it
    }

    ESP_LOGI(TAG, "Alarm window: %02d:%02d to %02d:%02d (%d → %d mins)",
             settings.alarm_hour, settings.alarm_minute,
             alarm_end_minutes / 60, alarm_end_minutes % 60,
             alarm_start_minutes, alarm_end_minutes);

    // Check if current time is within alarm window
    bool is_alarm_on = (current_total_minutes >= alarm_start_minutes) &&
                       (current_total_minutes <= alarm_end_minutes);

    if (is_alarm_on)
    {
        int passed_seconds = (current_total_minutes - alarm_start_minutes) * 60 + local_time.tm_sec;
        int total_duration_seconds = settings.duration_minutes * 60;

        // Clamp to [0.0, 1.0] to avoid >100% or negative
        sunrise_percentage = static_cast<double>(passed_seconds) / total_duration_seconds;
        if (sunrise_percentage < 0.0)
            sunrise_percentage = 0.0;
        if (sunrise_percentage > 1.0)
            sunrise_percentage = 1.0;

        ESP_LOGI(TAG, "Sunrise: %d sec passed of %d → %.2f%%",
                 passed_seconds, total_duration_seconds, sunrise_percentage * 100.0);
    }
    else
    {
        sunrise_percentage = 0.0;
    }

    ESP_LOGI(TAG, "Alarm active: %s", is_alarm_on ? "YES" : "NO");

    return is_alarm_on;
}

extern "C" void app_main(void)
{
    LEDStrip strip(17, 80, false);

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

    initialize_sntp();
    if (!obtain_time())
    {
        ESP_LOGW(TAG, "Failed to obtain time from NTP server!");
    }

    set_timezone("CET-1CEST,M3.5.0,M10.5.0/3"); // Vienna = Central European Time
    time_t now;
    time(&now);
    ESP_LOGI(TAG, "Time synchronized: %s", ctime(&now));

    ESP_LOGI(TAG, "Wi-Fi connected. Starting web server...");
    WebServer server(80);
    esp_err_t err = server.start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(err));
    }

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every 5 sec
        SunriseSettings settings = server.get_settings_copy();
        double sunrise_percentage = 0;

        ESP_LOGI("Main", "Sunrise active: R=%d G=%d B=%d, Brightness=%d, Duration=%d min", settings.red, settings.green, settings.blue, settings.brightness, settings.duration_minutes);

        if (settings.enabled && is_alarm_time(settings, sunrise_percentage))
        {
            ESP_LOGI("Main", "Sunrise execution...");
            int red = settings.red * sunrise_percentage;
            int green = settings.green * sunrise_percentage;
            int blue = settings.blue * sunrise_percentage;

            ESP_LOGI("Main", "red = %d, green = %d, blue = %d", red, green, blue);
            for (int i = 0; i < 80; i++)
            {
                strip.setPixel(i, red, green, blue);
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
