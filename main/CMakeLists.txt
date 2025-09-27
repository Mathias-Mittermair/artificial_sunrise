idf_component_register(
    SRCS "main.cpp"
    REQUIRES LEDStrip WebServer WifiManager Alarm Settings nvs_flash driver
)