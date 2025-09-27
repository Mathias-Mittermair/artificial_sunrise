idf_component_register(
    SRCS "main.cpp"
    REQUIRES LEDStrip WebServer WifiManager Alarm nvs_flash driver
)