#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <string>
#include <sstream>

static const char *TAG = "WebServer";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static WebServer* s_instance = nullptr;

WebServer::WebServer(uint16_t port) : port_(port), server_(nullptr) {}

WebServer::~WebServer()
{
    stop();
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const char resp[] = "<h1>Hello from ESP32 WebServer (FATFS)!</h1>";
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
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

std::string url_decode(const std::string& src) {
    std::string res;
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length()) {
            std::string hex = src.substr(i + 1, 2);
            res += static_cast<char>(std::stoi(hex, nullptr, 16));
            i += 2;
        } else if (src[i] == '+') {
            res += ' ';
        } else {
            res += src[i];
        }
    }
    return res;
}

void parse_params(const std::string& body, SunriseSettings& settings) {
    size_t pos = 0;
    std::string token;
    std::string input = body;
    while ((pos = input.find('&')) != std::string::npos) {
        token = input.substr(0, pos);
        if (token.find("brightness=") == 0) {
            settings.brightness = std::stoi(token.substr(11));
        } else if (token.find("red=") == 0) {
            settings.red = std::stoi(token.substr(4));
        } else if (token.find("green=") == 0) {
            settings.green = std::stoi(token.substr(6));
        } else if (token.find("blue=") == 0) {
            settings.blue = std::stoi(token.substr(5));
        } else if (token.find("duration=") == 0) {
            settings.duration_minutes = std::stoi(token.substr(9));
        } else if (token.find("enabled=") == 0) {
            settings.enabled = (token.substr(8) == "true" || token.substr(8) == "1");
        }
        input.erase(0, pos + 1);
    }
    // Last param
    if (!input.empty()) {
        if (input.find("brightness=") == 0) {
            settings.brightness = std::stoi(input.substr(11));
        } else if (input.find("red=") == 0) {
            settings.red = std::stoi(input.substr(4));
        } else if (input.find("green=") == 0) {
            settings.green = std::stoi(input.substr(6));
        } else if (input.find("blue=") == 0) {
            settings.blue = std::stoi(input.substr(5));
        } else if (input.find("duration=") == 0) {
            settings.duration_minutes = std::stoi(input.substr(9));
        } else if (input.find("enabled=") == 0) {
            settings.enabled = (input.substr(8) == "true" || input.substr(8) == "1");
        }
    }

    // Clamp values
    settings.brightness = std::max(0, std::min(255, settings.brightness));
    settings.red = std::max(0, std::min(255, settings.red));
    settings.green = std::max(0, std::min(255, settings.green));
    settings.blue = std::max(0, std::min(255, settings.blue));
    settings.duration_minutes = std::max(1, std::min(120, settings.duration_minutes));
}

esp_err_t WebServer::root_get_handler(httpd_req_t *req)
{
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>Artificial Sunrise</title></head><body>"
         << "<h1>Artificial Sunrise Settings</h1>"
         << "<form method='POST' action='/settings'>"
         << "<label>Brightness (0-255): <input type='number' name='brightness' value='"
         << s_instance->settings_.brightness << "' min='0' max='255'></label><br><br>"
         << "<label>Red (0-255): <input type='number' name='red' value='"
         << s_instance->settings_.red << "' min='0' max='255'></label><br><br>"
         << "<label>Green (0-255): <input type='number' name='green' value='"
         << s_instance->settings_.green << "' min='0' max='255'></label><br><br>"
         << "<label>Blue (0-255): <input type='number' name='blue' value='"
         << s_instance->settings_.blue << "' min='0' max='255'></label><br><br>"
         << "<label>Duration (min): <input type='number' name='duration' value='"
         << s_instance->settings_.duration_minutes << "' min='1' max='120'></label><br><br>"
         << "<label>Enabled: <input type='checkbox' name='enabled' value='1' "
         << (s_instance->settings_.enabled ? "checked" : "") << "></label><br><br>"
         << "<input type='submit' value='Save Settings'>"
         << "</form>"
         << "<p><i>Settings applied immediately.</i></p>"
         << "</body></html>";

    std::string response = html.str();
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

// POST /settings → update settings
esp_err_t WebServer::settings_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    std::string body = url_decode(std::string(buf));
    parse_params(body, s_instance->settings_);

    // Redirect back to form
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// Optional: GET /settings as JSON (for API)
esp_err_t WebServer::settings_get_handler(httpd_req_t *req)
{
    std::ostringstream json;
    json << "{"
         << "\"brightness\":" << s_instance->settings_.brightness << ","
         << "\"red\":" << s_instance->settings_.red << ","
         << "\"green\":" << s_instance->settings_.green << ","
         << "\"blue\":" << s_instance->settings_.blue << ","
         << "\"duration_minutes\":" << s_instance->settings_.duration_minutes << ","
         << "\"enabled\":" << (s_instance->settings_.enabled ? "true" : "false")
         << "}";

    httpd_resp_set_type(req, "application/json");
    std::string response = json.str();
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

esp_err_t WebServer::register_uri_handlers()
{
    // Store instance for static handlers
    s_instance = this;

    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = nullptr
    };
    httpd_register_uri_handler(server_, &root_uri);

    httpd_uri_t settings_post = {
        .uri       = "/settings",
        .method    = HTTP_POST,
        .handler   = settings_post_handler,
        .user_ctx  = nullptr
    };
    httpd_register_uri_handler(server_, &settings_post);

    httpd_uri_t settings_get = {
        .uri       = "/settings",
        .method    = HTTP_GET,
        .handler   = settings_get_handler,
        .user_ctx  = nullptr
    };
    httpd_register_uri_handler(server_, &settings_get);

    return ESP_OK;
}