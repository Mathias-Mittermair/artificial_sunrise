#pragma once

#include "led_strip.h"

class LEDStrip {
public:
    LEDStrip(int gpio_pin, int led_count, bool use_dma = false);
    ~LEDStrip();

    void setPixel(int index, uint8_t r, uint8_t g, uint8_t b);
    void refresh();
    void clear();

private:
    led_strip_handle_t handle;
    int count;
};
