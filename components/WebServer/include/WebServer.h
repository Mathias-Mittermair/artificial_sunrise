#pragma once

#include <cstdint>
#include <string>
#include "esp_err.h"
#include "esp_http_server.h"
#include "../../common/DataStructs.h"

class WebServer
{
public:
    explicit WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

    SunriseSettings get_settings_copy() const;
    esp_err_t handle_root_get(httpd_req_t *req);
    esp_err_t handle_settings_post(httpd_req_t *req);
    esp_err_t handle_settings_get(httpd_req_t *req);
    esp_err_t serve_static(httpd_req_t *req);

private:
    uint16_t port_;
    httpd_handle_t server_;
    SunriseSettings settings_;
    mutable SemaphoreHandle_t settings_mutex_;

    esp_err_t register_uri_handlers();
    std::string build_html_with_settings(const SunriseSettings &settings);

    static std::string replace_all(std::string str, const std::string &from, const std::string &to);
};
