#pragma once

#include <cstdint>
#include "esp_err.h"
#include "esp_http_server.h"

struct SunriseSettings {
    int brightness = 100;      // 0-255
    int red = 255;
    int green = 100;
    int blue = 0;
    int duration_minutes = 30; // Sunrise duration
    bool enabled = true;
};

class WebServer {
public:
    explicit WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

    const SunriseSettings& get_settings() const { return settings_; } 

private:
    esp_err_t register_uri_handlers();
    static esp_err_t root_get_handler(httpd_req_t *req);
    static esp_err_t settings_post_handler(httpd_req_t *req);
    static esp_err_t settings_get_handler(httpd_req_t *req);

    uint16_t port_;
    httpd_handle_t server_;
    SunriseSettings settings_;
};