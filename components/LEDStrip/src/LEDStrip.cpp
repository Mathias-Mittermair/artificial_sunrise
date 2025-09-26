#include "LEDStrip.h"
#include "esp_log.h"
#include "esp_err.h"

#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "LEDStrip";

LEDStrip::LEDStrip(int gpio_pin, int led_count, bool use_dma) : count(led_count) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = static_cast<int>(gpio_pin),
        .max_leds = static_cast<uint32_t>(led_count),
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = false }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = static_cast<size_t>(use_dma ? 1024 : 0),
        .flags = { .with_dma = use_dma }
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &handle));
    ESP_LOGI(TAG, "LED strip created");
}

LEDStrip::~LEDStrip() {
    led_strip_clear(handle);
}

void LEDStrip::setPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
    ESP_ERROR_CHECK(led_strip_set_pixel(handle, index, r, g, b));
}

void LEDStrip::refresh() {
    ESP_ERROR_CHECK(led_strip_refresh(handle));
}

void LEDStrip::clear() {
    ESP_ERROR_CHECK(led_strip_clear(handle));
}
