#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include <sstream>
#include <cstring>
#include <cassert>
#include <algorithm>

static const char *TAG = "WebServer";
static WebServer *s_instance = nullptr;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

std::string WebServer::replace_all(std::string str, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

WebServer::WebServer(uint16_t port) : port_(port), server_(nullptr) {
    settings_mutex_ = xSemaphoreCreateMutex();
    assert(settings_mutex_ != nullptr);
    settings_ = {};
}

WebServer::~WebServer() {
    stop();
    if (settings_mutex_) {
        vSemaphoreDelete(settings_mutex_);
    }
}

SunriseSettings WebServer::get_settings_copy() const {
    SunriseSettings copy;
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        copy = settings_;
        xSemaphoreGive(settings_mutex_);
    }
    return copy;
}

std::string WebServer::build_html_with_settings(const SunriseSettings &settings) {
    std::string html(reinterpret_cast<const char*>(index_html_start),
                     index_html_end - index_html_start);

    html = replace_all(html, "%RED%", std::to_string(settings.red));
    html = replace_all(html, "%GREEN%", std::to_string(settings.green));
    html = replace_all(html, "%BLUE%", std::to_string(settings.blue));
    html = replace_all(html, "%DURATION_MINUTES%", std::to_string(settings.duration_minutes));
    html = replace_all(html, "%DURATION_ON_BRIGHTEST%", std::to_string(settings.duration_on_brightest));
    html = replace_all(html, "%ALARM_HOUR%", std::to_string(settings.alarm_hour));
    html = replace_all(html, "%ALARM_MINUTE%", std::to_string(settings.alarm_minute));
    html = replace_all(html, "%ENABLED%", settings.enabled ? "checked" : "");
    html = replace_all(html, "%LIGHT_PREVIEW%", settings.light_preview ? "checked" : "");
    return html;
}

esp_err_t WebServer::handle_root_get(httpd_req_t *req) {
    SunriseSettings settings = get_settings_copy();
    std::string html = build_html_with_settings(settings);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServer::serve_static(httpd_req_t *req) {
    const uint8_t *start = nullptr;
    const uint8_t *end = nullptr;

    if (strcmp(req->uri, "/style.css") == 0) {
        start = style_css_start;
        end = style_css_end;
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, reinterpret_cast<const char*>(start), end - start);
    return ESP_OK;
}

esp_err_t WebServer::handle_settings_post(httpd_req_t *req) {
    size_t content_length = req->content_len;
    if (content_length == 0 || content_length > 4096) {
        ESP_LOGW(TAG, "Invalid Content-Length");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    std::string body;
    body.reserve(content_length);
    char buf[512];
    size_t received = 0;

    while (received < content_length) {
        size_t buffer_size = static_cast<size_t>(sizeof(buf));
        size_t to_read = std::min(buffer_size, content_length - received);
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        body.append(buf, ret);
        received += ret;
    }

    auto url_decode_value = [](const std::string &encoded) {
        std::string res;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                char hex_str[3] = {encoded[i + 1], encoded[i + 2], '\0'};
                char *end;
                long val = strtol(hex_str, &end, 16);
                if (end == hex_str + 2 && val >= 0 && val <= 255) {
                    res += static_cast<char>(val);
                    i += 2;
                    continue;
                }
            } else if (encoded[i] == '+') res += ' ';
            else res += encoded[i];
        }
        return res;
    };

    auto safe_stoi = [](const std::string &str, int def, int min_val, int max_val) {
        if (str.empty()) return def;
        char *end;
        long val = strtol(str.c_str(), &end, 10);
        if (*end != '\0') return def;
        if (val < min_val) return min_val;
        if (val > max_val) return max_val;
        return static_cast<int>(val);
    };

auto parse_params = [&](const std::string &body, SunriseSettings &settings) {
    bool new_enabled = false;
    bool new_light_preview = false;

    std::istringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        std::string key = pair.substr(0, eq);
        std::string value = url_decode_value(pair.substr(eq + 1));

        if (key == "red") settings.red = safe_stoi(value, settings.red, 0, 255);
        else if (key == "green") settings.green = safe_stoi(value, settings.green, 0, 255);
        else if (key == "blue") settings.blue = safe_stoi(value, settings.blue, 0, 255);
        else if (key == "duration_minutes") settings.duration_minutes = safe_stoi(value, settings.duration_minutes, 1, 120);
        else if (key == "duration_on_brightest") settings.duration_on_brightest = safe_stoi(value, settings.duration_on_brightest, 1, 120);
        else if (key == "alarm_hour") settings.alarm_hour = safe_stoi(value, settings.alarm_hour, 0, 23);
        else if (key == "alarm_minute") settings.alarm_minute = safe_stoi(value, settings.alarm_minute, 0, 59);
        else if (key == "enabled") new_enabled = (value == "1");
        else if (key == "light_preview") new_light_preview = (value == "1");
    }

    settings.enabled = new_enabled;
    settings.light_preview = new_light_preview;
};

    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        parse_params(body, settings_);
        xSemaphoreGive(settings_mutex_);
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::handle_settings_get(httpd_req_t *req) {
    SunriseSettings settings = get_settings_copy();
    std::ostringstream json;
    json << "{"
         << "\"red\":" << settings.red << ","
         << "\"green\":" << settings.green << ","
         << "\"blue\":" << settings.blue << ","
         << "\"duration_minutes\":" << settings.duration_minutes << ","
         << "\"duration_on_brightest\":" << settings.duration_on_brightest << ","
         << "\"alarm_hour\":" << settings.alarm_hour << ","
         << "\"alarm_minute\":" << settings.alarm_minute << ","
         << "\"enabled\":" << (settings.enabled ? "true" : "false")
         << "\"light_preview\":" << (settings.light_preview ? "true" : "false")
         << "}";
    httpd_resp_set_type(req, "application/json");
    std::string response = json.str();
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_root_get(req) : ESP_FAIL; }
static esp_err_t settings_post_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_settings_post(req) : ESP_FAIL; }
static esp_err_t settings_get_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_settings_get(req) : ESP_FAIL; }
static esp_err_t static_get_handler(httpd_req_t *req) { return s_instance ? s_instance->serve_static(req) : ESP_FAIL; }

esp_err_t WebServer::register_uri_handlers() {
    httpd_uri_t root_uri = {"/", HTTP_GET, root_get_handler, nullptr};
    httpd_register_uri_handler(server_, &root_uri);

    httpd_uri_t settings_post = {"/settings", HTTP_POST, settings_post_handler, nullptr};
    httpd_register_uri_handler(server_, &settings_post);

    httpd_uri_t settings_get = {"/settings", HTTP_GET, settings_get_handler, nullptr};
    httpd_register_uri_handler(server_, &settings_get);

    httpd_uri_t css_uri = {"/style.css", HTTP_GET, static_get_handler, nullptr};
    httpd_register_uri_handler(server_, &css_uri);

    return ESP_OK;
}

esp_err_t WebServer::start() {
    s_instance = this;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 8192;

    if (httpd_start(&server_, &config) != ESP_OK) return ESP_FAIL;
    return register_uri_handlers();
}

esp_err_t WebServer::stop() {
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }
    return ESP_OK;
}
