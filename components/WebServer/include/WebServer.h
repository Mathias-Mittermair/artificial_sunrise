#pragma once

#include "esp_err.h"

class WebServer {
public:
    WebServer(uint16_t port = 80);
    ~WebServer();

    esp_err_t start();
    esp_err_t stop();

private:
    uint16_t port_;
    void handle_root();
    void handle_set_alarm();
};
