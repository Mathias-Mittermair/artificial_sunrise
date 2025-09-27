#include "Alarm.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>

static const char *TAG = "ALARM";

namespace Alarm {

static void init_sntp() {
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

static void set_timezone(const char *tz) {
    setenv("TZ", tz, 1);
    tzset();
}

void init() {
    init_sntp();
    set_timezone("CET-1CEST,M3.5.0,M10.5.0/3");
    if (!obtain_time()) {
        ESP_LOGW(TAG, "Failed to obtain time from NTP server!");
    }
}

bool obtain_time(int timeout_sec) {
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < timeout_sec) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, timeout_sec);
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    return timeinfo.tm_year >= (2023 - 1900);
}

bool is_alarm_time(const SunriseSettings &settings, double &sunrise_percentage) {
    if (!settings.enabled) {
        sunrise_percentage = 0.0;
        return false;
    }

    time_t now = time(nullptr);
    struct tm local_time;
    localtime_r(&now, &local_time);

    int current_total_minutes = local_time.tm_hour * 60 + local_time.tm_min;
    // int alarm_start_minutes = settings.alarm_hour * 60 + settings.alarm_minute;
    // int alarm_end_minutes = alarm_start_minutes + settings.duration_minutes;

    // bool is_alarm_on = (current_total_minutes >= alarm_start_minutes) &&
    //                    (current_total_minutes <= alarm_end_minutes);

    // sunrise_percentage = 0.0;
    // if (is_alarm_on) {
    //     int passed_seconds = (current_total_minutes - alarm_start_minutes) * 60 + local_time.tm_sec;
    //     int total_duration_seconds = settings.duration_minutes * 60;
    //     sunrise_percentage = static_cast<double>(passed_seconds) / total_duration_seconds;
    //     sunrise_percentage = std::clamp(sunrise_percentage, 0.0, 1.0);
    // }

    //return is_alarm_on;
    return false;
}

}
