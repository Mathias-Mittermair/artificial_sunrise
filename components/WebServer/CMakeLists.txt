idf_component_register(
    SRCS "src/WebServer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server
)

target_add_binary_data(${COMPONENT_LIB} "src/html/index.html" TEXT)
target_add_binary_data(${COMPONENT_LIB} "src/html/style.css" TEXT)