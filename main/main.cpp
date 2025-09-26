#include "LEDStrip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFiManager.h"
#include "WebServer.h"
#include "esp_log.h"

extern "C" void app_main(void) {
    WiFiManager wifiManager;
    wifiManager.init();
    wifiManager.connect();

    WebServer server(80);
    server.start();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }


    // LEDStrip strip(17, 80, false);

    // while (true) {
    //     for (int i = 0; i < 80; i++) {
    //         strip.setPixel(i, 20, 0, 0);
    //     }
    //     strip.refresh();
    //     ESP_LOGI(TAG, "LEDs ON");

    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     strip.clear();
    //     ESP_LOGI(TAG, "LEDs OFF");

    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
