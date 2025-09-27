#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"

struct __attribute__((packed)) LowLevelSettings {
    uint8_t sunrise_red = 255;
    uint8_t sunrise_green = 120;
    uint8_t sunrise_blue = 150;
    uint16_t num_leds = 80;
    gpio_num_t pin_led = GPIO_NUM_17;
    gpio_num_t pin_alarm_switch = GPIO_NUM_18;
    gpio_num_t pin_light_switch = GPIO_NUM_19;
    uint16_t port = 80;
    uint16_t refresh_time = 1000;
    uint16_t cycle_sleep = 1000;
};

struct SunriseSettings {
    int red = 255;
    int green = 100;
    int blue = 0;

    bool light_preview = false;

    int duration_minutes = 5;
    int duration_on_brightest = 30;
    int alarm_hour = 7;
    int alarm_minute = 30;

    bool alarm_enabled = false;
    bool disable_hardware_switches = false;
};

class Settings {
public:
    static Settings& get();

    esp_err_t init();
    esp_err_t load();
    esp_err_t save();

    LowLevelSettings getSettings();
    esp_err_t setSettings(const LowLevelSettings &settings);

private:
    Settings();
    ~Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    LowLevelSettings settings_;
    SemaphoreHandle_t mutex_;
};
