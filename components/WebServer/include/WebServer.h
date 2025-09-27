#pragma once

#include <cstdint>
#include <string>
#include "esp_err.h"
#include "esp_http_server.h"
#include "Settings.h"

class WebServer
{
public:
    explicit WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

    SunriseSettings get_settings_copy() const;
    esp_err_t handle_root_get(httpd_req_t *req);
    esp_err_t handle_sunrise_get(httpd_req_t *req);
    esp_err_t handle_sunrise_post(httpd_req_t *req);
    esp_err_t serve_static(httpd_req_t *req);
    esp_err_t handle_low_level_settings_post(httpd_req_t *req);
    esp_err_t handle_low_level_settings_get(httpd_req_t *req);

    void set_alarm_enabled(bool enabled);
    bool get_alarm_enabled() const;
    void set_light_preview(bool enabled);
    bool get_light_preview() const;

private:
    uint16_t port_;
    SunriseSettings settings_;
    mutable SemaphoreHandle_t settings_mutex_;
    httpd_handle_t server_;

    esp_err_t register_uri_handlers();
    std::string generate_gpio_options(gpio_num_t selected_pin);
    std::string build_html_with_settings(const SunriseSettings &settings);
    std::string build_low_level_settings_html(const LowLevelSettings &s);

    static std::string replace_all(std::string str, const std::string &from, const std::string &to);
};
