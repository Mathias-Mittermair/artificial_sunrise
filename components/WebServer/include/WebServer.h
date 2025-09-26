#pragma once

#include <cstdint>
#include "esp_err.h"
#include "esp_http_server.h"

struct SunriseSettings
{
    int brightness = 100;
    int red = 255;
    int green = 100;
    int blue = 0;
    int duration_minutes = 30;
    int alarm_hour = 7;    // 0–23
    int alarm_minute = 30; // 0–59

    bool enabled = true;
};

class WebServer
{
public:
    explicit WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

    SunriseSettings get_settings_copy() const;

private:
    esp_err_t register_uri_handlers();
    static esp_err_t root_get_handler(httpd_req_t *req);
    static esp_err_t settings_post_handler(httpd_req_t *req);
    static esp_err_t settings_get_handler(httpd_req_t *req);

    uint16_t port_;
    httpd_handle_t server_;
    SunriseSettings settings_;
    mutable SemaphoreHandle_t settings_mutex_;
};