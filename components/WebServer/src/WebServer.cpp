#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include <string>
#include <sstream>

static const char *TAG = "WebServer";
static WebServer *s_instance = nullptr;

WebServer::WebServer(uint16_t port) : port_(port), server_(nullptr), settings_mutex_(nullptr)
{
    settings_mutex_ = xSemaphoreCreateMutex();
    assert(settings_mutex_ != nullptr);
}

WebServer::~WebServer()
{
    stop();
    if (settings_mutex_ != nullptr)
    {
        vSemaphoreDelete(settings_mutex_);
    }
}

esp_err_t WebServer::start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 8192;

    if (httpd_start(&server_, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server (port %d, stack %d)", config.server_port, config.stack_size);
        return ESP_FAIL;
    }

    esp_err_t reg_err = register_uri_handlers();
    if (reg_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register URI handlers");
        httpd_stop(server_);
        server_ = nullptr;
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
    return ESP_OK;
}

std::string url_decode(const std::string &src)
{
    std::string res;
    for (size_t i = 0; i < src.length(); ++i)
    {
        if (src[i] == '%' && i + 2 < src.length())
        {
            std::string hex = src.substr(i + 1, 2);
            res += static_cast<char>(std::stoi(hex, nullptr, 16));
            i += 2;
        }
        else if (src[i] == '+')
        {
            res += ' ';
        }
        else
        {
            res += src[i];
        }
    }
    return res;
}

void parse_params(const std::string &body, SunriseSettings &settings)
{
    // Initialize a temporary struct or reset only what's provided?
    // We'll parse only what's in the body, but ensure all fields are covered.

    // Default: keep existing values unless overridden
    // But since we always send all fields (thanks to hidden input), safe to parse all.

    size_t pos = 0;
    std::string token;
    std::string input = body;

    // Helper lambda to extract value after '='
    auto get_value = [](const std::string& param, const std::string& key) -> std::string {
        if (param.substr(0, key.length()) == key) {
            return param.substr(key.length());
        }
        return "";
    };

    while ((pos = input.find('&')) != std::string::npos)
    {
        token = input.substr(0, pos);
        if (!token.empty()) {
            if (token.find("brightness=") == 0) {
                settings.brightness = std::stoi(get_value(token, "brightness="));
            } else if (token.find("red=") == 0) {
                settings.red = std::stoi(get_value(token, "red="));
            } else if (token.find("green=") == 0) {
                settings.green = std::stoi(get_value(token, "green="));
            } else if (token.find("blue=") == 0) {
                settings.blue = std::stoi(get_value(token, "blue="));
            } else if (token.find("duration=") == 0) {
                settings.duration_minutes = std::stoi(get_value(token, "duration="));
            } else if (token.find("alarm_hour=") == 0) {
                settings.alarm_hour = std::stoi(get_value(token, "alarm_hour="));
            } else if (token.find("alarm_minute=") == 0) {
                settings.alarm_minute = std::stoi(get_value(token, "alarm_minute="));
            } else if (token.find("enabled=") == 0) {
                std::string val = get_value(token, "enabled=");
                settings.enabled = (val == "1" || val == "true");
            }
        }
        input.erase(0, pos + 1);
    }

    // Last parameter
    if (!input.empty()) {
        if (input.find("brightness=") == 0) {
            settings.brightness = std::stoi(get_value(input, "brightness="));
        } else if (input.find("red=") == 0) {
            settings.red = std::stoi(get_value(input, "red="));
        } else if (input.find("green=") == 0) {
            settings.green = std::stoi(get_value(input, "green="));
        } else if (input.find("blue=") == 0) {
            settings.blue = std::stoi(get_value(input, "blue="));
        } else if (input.find("duration=") == 0) {
            settings.duration_minutes = std::stoi(get_value(input, "duration="));
        } else if (input.find("alarm_hour=") == 0) {
            settings.alarm_hour = std::stoi(get_value(input, "alarm_hour="));
        } else if (input.find("alarm_minute=") == 0) {
            settings.alarm_minute = std::stoi(get_value(input, "alarm_minute="));
        } else if (input.find("enabled=") == 0) {
            std::string val = get_value(input, "enabled=");
            settings.enabled = (val == "1" || val == "true");
        }
    }

    // Clamp values
    settings.brightness = std::max(0, std::min(255, settings.brightness));
    settings.red = std::max(0, std::min(255, settings.red));
    settings.green = std::max(0, std::min(255, settings.green));
    settings.blue = std::max(0, std::min(255, settings.blue));
    settings.duration_minutes = std::max(1, std::min(120, settings.duration_minutes));
    settings.alarm_hour = std::max(0, std::min(23, settings.alarm_hour));
    settings.alarm_minute = std::max(0, std::min(59, settings.alarm_minute));
}

esp_err_t WebServer::root_get_handler(httpd_req_t *req)
{
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Artificial Sunrise</title></head><body>"
         << "<h1>Artificial Sunrise Settings</h1>"
         << "<form method='POST' action='/settings'>"
         << "<label>Brightness: <input type='number' name='brightness' value='" << s_instance->settings_.brightness << "' min='0' max='255'></label><br>"
         << "<label>Red: <input type='number' name='red' value='" << s_instance->settings_.red << "' min='0' max='255'></label><br>"
         << "<label>Green: <input type='number' name='green' value='" << s_instance->settings_.green << "' min='0' max='255'></label><br>"
         << "<label>Blue: <input type='number' name='blue' value='" << s_instance->settings_.blue << "' min='0' max='255'></label><br>"
         << "<label>Duration: <input type='number' name='duration' value='" << s_instance->settings_.duration_minutes << "' min='1' max='120'></label><br>"
         << "<label>Alarm Hour: <input type='number' name='alarm_hour' value='" << s_instance->settings_.alarm_hour << "' min='0' max='23'></label><br>"
         << "<label>Alarm Minute: <input type='number' name='alarm_minute' value='" << s_instance->settings_.alarm_minute << "' min='0' max='59'></label><br>"
         << "<label>Enabled: <input type='checkbox' name='enabled' value='1' " << (s_instance->settings_.enabled ? "checked" : "") << "></label><br>"
         << "<input type='submit' value='Save'>"
         << "</form></body></html>";

    std::string response = html.str();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

SunriseSettings WebServer::get_settings_copy() const
{
    SunriseSettings copy;
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        copy = settings_;
        xSemaphoreGive(settings_mutex_);
    }
    return copy;
}

// POST /settings → update settings
esp_err_t WebServer::settings_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    std::string body = url_decode(std::string(buf));

    if (xSemaphoreTake(s_instance->settings_mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        parse_params(body, s_instance->settings_);
        xSemaphoreGive(s_instance->settings_mutex_);
    }

    // Redirect back to form
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// Optional: GET /settings as JSON (for API)
esp_err_t WebServer::settings_get_handler(httpd_req_t *req)
{
    SunriseSettings settings;
    if (xSemaphoreTake(s_instance->settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        settings = s_instance->settings_;
        xSemaphoreGive(s_instance->settings_mutex_);
    }
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
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = nullptr};
    httpd_register_uri_handler(server_, &root_uri);

    httpd_uri_t settings_post = {
        .uri = "/settings",
        .method = HTTP_POST,
        .handler = settings_post_handler,
        .user_ctx = nullptr};
    httpd_register_uri_handler(server_, &settings_post);

    httpd_uri_t settings_get = {
        .uri = "/settings",
        .method = HTTP_GET,
        .handler = settings_get_handler,
        .user_ctx = nullptr};
    httpd_register_uri_handler(server_, &settings_get);

    return ESP_OK;
}