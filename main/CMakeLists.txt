idf_component_register(
    SRCS "main.cpp"
    REQUIRES LEDStrip WebServer esp_wifi esp_netif nvs_flash
)