#include "Settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "Settings";

Settings& Settings::get() {
    static Settings instance;
    return instance;
}

Settings::Settings() {
    mutex_ = xSemaphoreCreateMutex();
}

Settings::~Settings() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

esp_err_t Settings::init()
{
    return load();
}

esp_err_t Settings::load() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS öffnen fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(settings_);
    err = nvs_get_blob(nvs_handle, "lls", &settings_, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Keine gespeicherten Settings gefunden, Standardwerte werden genutzt");
        err = ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fehler beim Laden der Settings: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t Settings::save() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs_handle, "lls", &settings_, sizeof(settings_));
    if (err == ESP_OK) err = nvs_commit(nvs_handle);

    nvs_close(nvs_handle);
    return err;
}

LowLevelSettings Settings::getSettings() {
    LowLevelSettings copy;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = settings_;
        xSemaphoreGive(mutex_);
    }
    return copy;
}

esp_err_t Settings::setSettings(const LowLevelSettings &settings) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE)
        return ESP_FAIL;

    settings_ = settings;
    xSemaphoreGive(mutex_);

    return save();
}
