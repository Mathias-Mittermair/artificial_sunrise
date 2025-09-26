#pragma once

#include <cstdint>
#include "esp_err.h"
#include "esp_http_server.h"

class WebServer {
public:
    explicit WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

private:
    esp_err_t register_uri_handlers();

    uint16_t port_;
    httpd_handle_t server_;  // ← NOT a pointer! It's an opaque handle (like FILE*)
};