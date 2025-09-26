#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

class WiFiManager {
public:
    void init();
    esp_err_t connect();
    bool is_connected();
};

#endif