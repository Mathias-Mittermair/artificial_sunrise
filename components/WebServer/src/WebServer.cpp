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
extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
extern const uint8_t settings_html_end[] asm("_binary_settings_html_end");

static esp_err_t root_get_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_root_get(req) : ESP_FAIL; }
static esp_err_t sunrise_get_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_sunrise_get(req) : ESP_FAIL; }
static esp_err_t sunrise_post_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_sunrise_post(req) : ESP_FAIL; }
static esp_err_t settings_post_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_low_level_settings_post(req) : ESP_FAIL; }
static esp_err_t settings_get_handler(httpd_req_t *req) { return s_instance ? s_instance->handle_low_level_settings_get(req) : ESP_FAIL; }
static esp_err_t static_get_handler(httpd_req_t *req) { return s_instance ? s_instance->serve_static(req) : ESP_FAIL; }

std::string WebServer::replace_all(std::string str, const std::string &from, const std::string &to)
{
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos)
    {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

WebServer::WebServer(uint16_t port)
    : port_(port), settings_(), settings_mutex_(nullptr), server_(nullptr)
{
    settings_mutex_ = xSemaphoreCreateMutex();
    assert(settings_mutex_ != nullptr);
}

WebServer::~WebServer()
{
    stop();
    if (settings_mutex_)
    {
        vSemaphoreDelete(settings_mutex_);
    }
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

std::string WebServer::build_html_with_settings(const SunriseSettings &settings)
{
    std::string html(reinterpret_cast<const char *>(index_html_start),
                     index_html_end - index_html_start);

    html = replace_all(html, "%RED%", std::to_string(settings.red));
    html = replace_all(html, "%GREEN%", std::to_string(settings.green));
    html = replace_all(html, "%BLUE%", std::to_string(settings.blue));
    html = replace_all(html, "%DURATION_MINUTES%", std::to_string(settings.duration_minutes));
    html = replace_all(html, "%DURATION_ON_BRIGHTEST%", std::to_string(settings.duration_on_brightest));
    html = replace_all(html, "%ALARM_HOUR%", std::to_string(settings.alarm_hour));
    html = replace_all(html, "%ALARM_MINUTE%", std::to_string(settings.alarm_minute));
    html = replace_all(html, "%ENABLED%", settings.alarm_enabled ? "checked" : "");
    html = replace_all(html, "%LIGHT_PREVIEW%", settings.light_preview ? "checked" : "");
    html = replace_all(html, "%DISABLE_SETTINGS%", settings.disable_hardware_switches ? "checked" : "");
    return html;
}

std::string WebServer::build_low_level_settings_html(const LowLevelSettings &s)
{
    std::string html(reinterpret_cast<const char *>(settings_html_start),
                     settings_html_end - settings_html_start);

    html = replace_all(html, "%SUNRISE_RED%", std::to_string(s.sunrise_red));
    html = replace_all(html, "%SUNRISE_GREEN%", std::to_string(s.sunrise_green));
    html = replace_all(html, "%SUNRISE_BLUE%", std::to_string(s.sunrise_blue));
    html = replace_all(html, "%NUM_LEDS%", std::to_string(s.num_leds));
    html = replace_all(html, "%PORT%", std::to_string(s.port));
    html = replace_all(html, "%REFRESH_TIME%", std::to_string(s.refresh_time));
    html = replace_all(html, "%CYCLE_SLEEP%", std::to_string(s.cycle_sleep));

    html = replace_all(html, "%PIN_LED_OPTIONS%", generate_gpio_options(s.pin_led));
    html = replace_all(html, "%PIN_ALARM_OPTIONS%", generate_gpio_options(s.pin_alarm_switch));
    html = replace_all(html, "%PIN_LIGHT_OPTIONS%", generate_gpio_options(s.pin_light_switch));

    return html;
}

std::string WebServer::generate_gpio_options(gpio_num_t selected_pin)
{
    const int gpio_options[] = {0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
    std::ostringstream options;

    for (int pin : gpio_options)
    {
        options << "<option value='" << pin << "'";
        if (pin == int(selected_pin))
            options << " selected";
        options << ">GPIO" << pin << "</option>";
    }
    return options.str();
}

esp_err_t WebServer::handle_root_get(httpd_req_t *req)
{
    SunriseSettings settings = get_settings_copy();
    std::string html = build_html_with_settings(settings);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServer::serve_static(httpd_req_t *req)
{
    const uint8_t *start = nullptr;
    const uint8_t *end = nullptr;

    if (strcmp(req->uri, "/style.css") == 0)
    {
        start = style_css_start;
        end = style_css_end;
    }
    else
    {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, reinterpret_cast<const char *>(start), end - start);
    return ESP_OK;
}

esp_err_t WebServer::handle_sunrise_get(httpd_req_t *req)
{
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
         << "\"enabled\":" << (settings.alarm_enabled ? "true" : "false") << ","
         << "\"light_preview\":" << (settings.light_preview ? "true" : "false") << ","
         << "\"disable_hardware_switches\":" << (settings.disable_hardware_switches ? "true" : "false")
         << "}";
    httpd_resp_set_type(req, "application/json");
    std::string response = json.str();
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

esp_err_t WebServer::handle_sunrise_post(httpd_req_t *req)
{
    size_t content_length = req->content_len;
    if (content_length == 0 || content_length > 4096)
    {
        ESP_LOGW(TAG, "Invalid Content-Length");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    std::string body;
    body.reserve(content_length);
    char buf[512];
    size_t received = 0;

    while (received < content_length)
    {
        size_t buffer_size = static_cast<size_t>(sizeof(buf));
        size_t to_read = std::min(buffer_size, content_length - received);
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0)
        {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        body.append(buf, ret);
        received += ret;
    }

    auto url_decode_value = [](const std::string &encoded)
    {
        std::string res;
        for (size_t i = 0; i < encoded.length(); ++i)
        {
            if (encoded[i] == '%' && i + 2 < encoded.length())
            {
                char hex_str[3] = {encoded[i + 1], encoded[i + 2], '\0'};
                char *end;
                long val = strtol(hex_str, &end, 16);
                if (end == hex_str + 2 && val >= 0 && val <= 255)
                {
                    res += static_cast<char>(val);
                    i += 2;
                    continue;
                }
            }
            else if (encoded[i] == '+')
                res += ' ';
            else
                res += encoded[i];
        }
        return res;
    };

    auto safe_stoi = [](const std::string &str, int def, int min_val, int max_val)
    {
        if (str.empty())
            return def;
        char *end;
        long val = strtol(str.c_str(), &end, 10);
        if (*end != '\0')
            return def;
        if (val < min_val)
            return min_val;
        if (val > max_val)
            return max_val;
        return static_cast<int>(val);
    };

    auto parse_params = [&](const std::string &body, SunriseSettings &settings)
    {
        bool new_enabled = false;
        bool new_light_preview = false;
        bool new_disable_hardware_switches = false;

        std::istringstream ss(body);
        std::string pair;
        while (std::getline(ss, pair, '&'))
        {
            size_t eq = pair.find('=');
            if (eq == std::string::npos)
                continue;
            std::string key = pair.substr(0, eq);
            std::string value = url_decode_value(pair.substr(eq + 1));

            if (key == "red")
                settings.red = safe_stoi(value, settings.red, 0, 255);
            else if (key == "green")
                settings.green = safe_stoi(value, settings.green, 0, 255);
            else if (key == "blue")
                settings.blue = safe_stoi(value, settings.blue, 0, 255);
            else if (key == "duration_minutes")
                settings.duration_minutes = safe_stoi(value, settings.duration_minutes, 1, 120);
            else if (key == "duration_on_brightest")
                settings.duration_on_brightest = safe_stoi(value, settings.duration_on_brightest, 1, 120);
            else if (key == "alarm_hour")
                settings.alarm_hour = safe_stoi(value, settings.alarm_hour, 0, 23);
            else if (key == "alarm_minute")
                settings.alarm_minute = safe_stoi(value, settings.alarm_minute, 0, 59);
            else if (key == "enabled")
                new_enabled = (value == "1");
            else if (key == "light_preview")
                new_light_preview = (value == "1");
            else if (key == "disable_hardware_switches")
                new_disable_hardware_switches = (value == "1");
        }

        settings.alarm_enabled = new_enabled;
        settings.light_preview = new_light_preview;
        settings.disable_hardware_switches = new_disable_hardware_switches;
    };

    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        parse_params(body, settings_);
        xSemaphoreGive(settings_mutex_);
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t WebServer::register_uri_handlers()
{
    httpd_uri_t root_get = {"/", HTTP_GET, root_get_handler, nullptr};
    httpd_register_uri_handler(server_, &root_get);

    httpd_uri_t sunrise_get = {"/sunrise", HTTP_GET, sunrise_get_handler, nullptr};
    httpd_register_uri_handler(server_, &sunrise_get);

    httpd_uri_t sunrise_post = {"/sunrise", HTTP_POST, sunrise_post_handler, nullptr};
    httpd_register_uri_handler(server_, &sunrise_post);

    httpd_uri_t css_uri = {"/style.css", HTTP_GET, static_get_handler, nullptr};
    httpd_register_uri_handler(server_, &css_uri);

    httpd_uri_t low_level_get = {"/settings", HTTP_GET, settings_get_handler, nullptr};
    httpd_register_uri_handler(server_, &low_level_get);

    httpd_uri_t low_level_post = {"/settings", HTTP_POST, settings_post_handler, nullptr};
    httpd_register_uri_handler(server_, &low_level_post);

    return ESP_OK;
}

esp_err_t WebServer::start()
{
    s_instance = this;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port_;
    config.stack_size = 8192;

    if (httpd_start(&server_, &config) != ESP_OK)
        return ESP_FAIL;
    return register_uri_handlers();
}

esp_err_t WebServer::stop()
{
    if (server_)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
    return ESP_OK;
}

void WebServer::set_alarm_enabled(bool enabled)
{
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        if (!settings_.disable_hardware_switches)
            settings_.alarm_enabled = enabled;
        xSemaphoreGive(settings_mutex_);
    }
}

bool WebServer::get_alarm_enabled() const
{
    bool enabled = false;
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        enabled = settings_.alarm_enabled;
        xSemaphoreGive(settings_mutex_);
    }
    return enabled;
}

void WebServer::set_light_preview(bool enabled)
{
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        if (!settings_.disable_hardware_switches)
            settings_.light_preview = enabled;
        xSemaphoreGive(settings_mutex_);
    }
}

bool WebServer::get_light_preview() const
{
    bool light_preview = false;
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        light_preview = settings_.light_preview;
        xSemaphoreGive(settings_mutex_);
    }
    return light_preview;
}

esp_err_t WebServer::handle_low_level_settings_get(httpd_req_t *req)
{
    LowLevelSettings s = Settings::get().getSettings();
    std::string html = build_low_level_settings_html(s);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

esp_err_t WebServer::handle_low_level_settings_post(httpd_req_t *req)
{
    size_t content_length = req->content_len;
    if (content_length == 0 || content_length > 4096)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request: invalid content length");
        return ESP_FAIL;
    }

    std::string body;
    char buf[512];
    size_t received = 0;
    while (received < content_length)
    {
        size_t buffer_size = static_cast<size_t>(sizeof(buf));
        size_t to_read = std::min(buffer_size, content_length - received);
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0)
        {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        body.append(buf, ret);
        received += ret;
    }

    // URL-Decoding-Hilfsfunktion
    auto url_decode = [](const std::string& encoded) -> std::string {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                char hex_str[3] = {encoded[i + 1], encoded[i + 2], '\0'};
                char *end;
                long val = strtol(hex_str, &end, 16);
                if (end == hex_str + 2 && val >= 0 && val <= 255) {
                    decoded += static_cast<char>(val);
                    i += 2;
                    continue;
                }
            }
            if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        return decoded;
    };

    // Sichere Konvertierung mit Standardwert und Clamping
    auto safe_stoi = [](const std::string &str, int def, int min_val, int max_val) -> int {
        if (str.empty()) return def;
        char *end;
        long val = strtol(str.c_str(), &end, 10);
        if (*end != '\0') return def;
        if (val < min_val) return min_val;
        if (val > max_val) return max_val;
        return static_cast<int>(val);
    };

    // GPIO-Validierung (ESP32-spezifisch)
    auto is_valid_gpio = [](int pin) -> bool {
        if (pin < 0 || pin > 39) return false;
        // GPIO 6–11: reserviert für SPI-Flash → NICHT nutzbar!
        if (pin >= 6 && pin <= 11) return false;
        // Optional: weitere ungültige Pins (z. B. RTC-only) können hier hinzugefügt werden
        return true;
    };

    LowLevelSettings new_settings = Settings::get().getSettings();

    std::istringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;

        std::string key = url_decode(pair.substr(0, eq));
        std::string value = url_decode(pair.substr(eq + 1));

        if (key == "sunrise_red")
            new_settings.sunrise_red = static_cast<uint8_t>(safe_stoi(value, new_settings.sunrise_red, 0, 255));
        else if (key == "sunrise_green")
            new_settings.sunrise_green = static_cast<uint8_t>(safe_stoi(value, new_settings.sunrise_green, 0, 255));
        else if (key == "sunrise_blue")
            new_settings.sunrise_blue = static_cast<uint8_t>(safe_stoi(value, new_settings.sunrise_blue, 0, 255));
        else if (key == "num_leds")
            new_settings.num_leds = static_cast<uint16_t>(safe_stoi(value, new_settings.num_leds, 1, 10000));
        else if (key == "refresh_time")
            new_settings.refresh_time = static_cast<uint16_t>(safe_stoi(value, new_settings.refresh_time, 1, 10000));
        else if (key == "cycle_sleep")
            new_settings.cycle_sleep = static_cast<uint16_t>(safe_stoi(value, new_settings.cycle_sleep, 1, 10000));
        else if (key == "port")
            new_settings.port = static_cast<uint16_t>(safe_stoi(value, new_settings.port, 1, 65535));
        else if (key == "pin_led") {
            int pin_val = safe_stoi(value, static_cast<int>(new_settings.pin_led), 0, 39);
            if (is_valid_gpio(pin_val)) {
                new_settings.pin_led = static_cast<gpio_num_t>(pin_val);
            }
        }
        else if (key == "pin_alarm_switch") {
            int pin_val = safe_stoi(value, static_cast<int>(new_settings.pin_alarm_switch), 0, 39);
            if (is_valid_gpio(pin_val)) {
                new_settings.pin_alarm_switch = static_cast<gpio_num_t>(pin_val);
            }
        }
        else if (key == "pin_light_switch") {
            int pin_val = safe_stoi(value, static_cast<int>(new_settings.pin_light_switch), 0, 39);
            if (is_valid_gpio(pin_val)) {
                new_settings.pin_light_switch = static_cast<gpio_num_t>(pin_val);
            }
        }
    }

    esp_err_t err = Settings::get().setSettings(new_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE("WebServer", "Fehler beim Speichern der Low-Level-Settings: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Fehler beim Speichern der Einstellungen");
        return ESP_FAIL;
    }

    // Erfolgsnachricht
    std::string msg = "<html><head><meta charset='UTF-8'></head><body>"
                      "<h3>✅ Einstellungen gespeichert!</h3>"
                      "<p>Neustart erforderlich, damit die Änderungen wirksam werden.</p>"
                      "<a href='/'>Zurück zur Startseite</a></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, msg.c_str(), msg.length());

    return ESP_OK;
}