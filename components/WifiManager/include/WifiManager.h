#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

class WiFiManager {
public:
    esp_err_t init();
    esp_err_t connect();
};

#endif
