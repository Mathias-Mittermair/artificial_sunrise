#include "WebServer.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char* TAG = "WebServer";

WebServer::WebServer(uint16_t port) : port_(port) {}

WebServer::~WebServer() {
    stop();
}

esp_err_t WebServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = [](httpd_req_t *req) -> esp_err_t {
            const char resp[] = "<h1>Hello from ESP32 WebServer!</h1>";
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx  = nullptr
    };

    httpd_register_uri_handler(server, &root_uri);

    ESP_LOGI(TAG, "Web server started on port %d", port_);
    return ESP_OK;
}

esp_err_t WebServer::stop() {
    ESP_LOGI(TAG, "Web server stopped");
    return ESP_OK;
}
