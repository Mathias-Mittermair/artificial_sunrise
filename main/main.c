/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

#define LED_STRIP_USE_DMA  0

#if LED_STRIP_USE_DMA
#define LED_STRIP_LED_COUNT 256
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024
#else
#define LED_STRIP_LED_COUNT 80
#define LED_STRIP_MEMORY_BLOCK_WORDS 0
#endif

#define LED_STRIP_GPIO_PIN  17
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

static const char *TAG = "led_strip_example";

led_strip_handle_t configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS,
        .flags = {
            .with_dma = LED_STRIP_USE_DMA,
        }
    };

    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "LED strip object created");
    return led_strip;
}

void app_main(void)
{
    led_strip_handle_t led_strip = configure_led();
    bool led_on_off = false;

    ESP_LOGI(TAG, "Start LED strip test");

    while (1) {
        if (led_on_off) {
            for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 255, 0, 0)); // Rot
            }
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            ESP_LOGI(TAG, "LEDs ON (Red)");
        } else {
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
            ESP_LOGI(TAG, "LEDs OFF");
        }
        led_on_off = !led_on_off;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
