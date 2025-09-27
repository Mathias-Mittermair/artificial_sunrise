#include "Settings.h"
#include "nvs_flash.h"
#include "nvs.h"

#define STORAGE_NAMESPACE "LOW_LEVEL_SETTINGS"

Settings& Settings::get() {
    static Settings instance;
    return instance;
}

esp_err_t Settings::init() {
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK && err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return err;
    }
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    return load();
}

esp_err_t Settings::load() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    uint8_t u8_val;
    uint16_t u16_val;

    if (nvs_get_u8(handle, "sunrise_red", &u8_val) == ESP_OK) settings_.sunrise_red = u8_val;
    if (nvs_get_u8(handle, "sunrise_green", &u8_val) == ESP_OK) settings_.sunrise_green = u8_val;
    if (nvs_get_u8(handle, "sunrise_blue", &u8_val) == ESP_OK) settings_.sunrise_blue = u8_val;
    if (nvs_get_u16(handle, "num_leds", &u16_val) == ESP_OK) settings_.num_leds = u16_val;
    if (nvs_get_u16(handle, "refresh_time", &u16_val) == ESP_OK) settings_.refresh_time = u16_val;
    if (nvs_get_u16(handle, "cycle_sleep", &u16_val) == ESP_OK) settings_.cycle_sleep = u16_val;

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t Settings::save() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    nvs_set_u8(handle, "sunrise_red", settings_.sunrise_red);
    nvs_set_u8(handle, "sunrise_green", settings_.sunrise_green);
    nvs_set_u8(handle, "sunrise_blue", settings_.sunrise_blue);
    nvs_set_u16(handle, "num_leds", settings_.num_leds);
    nvs_set_u16(handle, "refresh_time", settings_.refresh_time);
    nvs_set_u16(handle, "cycle_sleep", settings_.cycle_sleep);

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

LowLevelSettings& Settings::getSettings() {
    return settings_;
}

void Settings::setSettings(const LowLevelSettings& s) {
    settings_ = s;
}
