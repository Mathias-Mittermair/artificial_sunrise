#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

static const char *TAG = "WebServer";

// Global wear levelling handle (only one instance expected)
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

WebServer::WebServer(uint16_t port) : port_(port), server_(nullptr) {}

WebServer::~WebServer()
{
    stop();
}

// Static HTTP handler (C-compatible)
static esp_err_t root_handler(httpd_req_t *req)
{
    const char resp[] = "<h1>Hello from ESP32 WebServer (FATFS)!</h1>";
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebServer::register_uri_handlers()
{
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = nullptr
    };
    return httpd_register_uri_handler(server_, &root_uri);
}

esp_err_t WebServer::start()
{
    const char *base_path = "/www";
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat = false
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "FATFS mounted at %s", base_path);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 8192;

    // ✅ CORRECT: Pass ADDRESS of server_ (because httpd_start expects httpd_handle_t*)
    if (httpd_start(&server_, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server (port %d, stack %d)",
                 config.server_port, config.stack_size);
        esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
        return ESP_FAIL;
    }

    esp_err_t reg_err = register_uri_handlers();
    if (reg_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register URI handlers");
        httpd_stop(server_);
        server_ = nullptr;
        esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
        return reg_err;
    }

    ESP_LOGI(TAG, "Web server started on port %d", port_);
    return ESP_OK;
}

esp_err_t WebServer::stop()
{
    if (server_ != nullptr)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
    if (s_wl_handle != WL_INVALID_HANDLE)
    {
        esp_vfs_fat_spiflash_unmount_rw_wl("/www", s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
    }
    return ESP_OK;
}