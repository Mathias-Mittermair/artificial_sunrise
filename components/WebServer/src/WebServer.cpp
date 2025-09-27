#include "WebServer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include <sstream>
#include <limits>
#include <cstring>
#include <cassert>

static const char *TAG = "WebServer";
static WebServer *s_instance = nullptr;

// Hilfsfunktion: URL-Decodierung für Werte (nicht für gesamten Body!)
std::string url_decode_value(const std::string &encoded)
{
    std::string res;
    for (size_t i = 0; i < encoded.length(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.length())
        {
            char hex_str[3] = {encoded[i + 1], encoded[i + 2], '\0'};
            if (std::isxdigit((unsigned char)hex_str[0]) && std::isxdigit((unsigned char)hex_str[1]))
            {
                char *end;
                long val = strtol(hex_str, &end, 16);
                if (end == hex_str + 2 && val >= 0 && val <= 255)
                {
                    res += static_cast<char>(val);
                    i += 2;
                    continue;
                }
            }
            res += '%';
        }
        else if (encoded[i] == '+')
        {
            res += ' ';
        }
        else
        {
            res += encoded[i];
        }
    }
    return res;
}

int safe_stoi(const std::string &str, int default_val = 0, int min_val = 0, int max_val = 255)
{
    if (str.empty())
        return default_val;

    const char *cstr = str.c_str();
    char *end;
    long val = strtol(cstr, &end, 10);

    if (end == cstr || *end != '\0')
    {
        return default_val;
    }

    if (val < min_val)
        return min_val;
    if (val > max_val)
        return max_val;
    return static_cast<int>(val);
}

std::string get_param(const std::string &body, const std::string &key)
{
    std::istringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key)
        {
            return url_decode_value(pair.substr(eq + 1));
        }
    }
    return "";
}

void parse_params(const std::string &body, SunriseSettings &settings)
{
    std::istringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;

        std::string key = pair.substr(0, eq);
        std::string value = (eq + 1 < pair.length()) ? pair.substr(eq + 1) : "";
        value = url_decode_value(value);

        if (key == "red") settings.red = safe_stoi(value, settings.red, 0, 255);
        else if (key == "green") settings.green = safe_stoi(value, settings.green, 0, 255);
        else if (key == "blue") settings.blue = safe_stoi(value, settings.blue, 0, 255);
        else if (key == "duration_minutes") settings.duration_minutes = safe_stoi(value, settings.duration_minutes, 1, 120);
        else if (key == "duration_on_brightest") settings.duration_on_brightest = safe_stoi(value, settings.duration_on_brightest, 1, 120);
        else if (key == "alarm_hour") settings.alarm_hour = safe_stoi(value, settings.alarm_hour, 0, 23);
        else if (key == "alarm_minute") settings.alarm_minute = safe_stoi(value, settings.alarm_minute, 0, 59);
        else if (key == "enabled") settings.enabled = (value == "1");
    }
}

WebServer::WebServer(uint16_t port) : port_(port), server_(nullptr), settings_mutex_(nullptr)
{
    settings_ = {}; // explizite Initialisierung
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
    s_instance = this; // sicherstellen, dass Handler auf diese Instanz zeigen

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

esp_err_t WebServer::handle_root_get(httpd_req_t *req)
{
    SunriseSettings settings = get_settings_copy();

    std::ostringstream html;
    html << R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Artificial Sunrise</title>
    <style>
        :root {
            --bg-body: #f8f9fa;
            --bg-card: #ffffff;
            --border: #e0e0e0;
            --text: #333333;
            --text-muted: #666666;
            --accent: #b97864ff;
            --switch-bg: #b97864ff;
            --switch-checked: #64a1b9ff;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background: var(--bg-body);
            color: var(--text);
        }
        .container {
            max-width: 700px;
            margin: 0 auto;
            background: var(--bg-card);
            border-radius: 16px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.05);
            overflow: hidden;
        }
        .header {
            text-align: center;
            padding: 20px;
            background: var(--accent);
            color: white;
        }
        .section {
            padding: 20px;
            border-bottom: 1px solid var(--border);
        }
        .section:last-child {
            border-bottom: none;
        }
        .section-title {
            font-size: 1.3em;
            margin: 0 0 16px 0;
            color: var(--accent);
            font-weight: 600;
        }
        .form-group {
            margin-bottom: 16px;
        }
        label {
            display: block;
            margin-bottom: 6px;
            font-weight: 600;
            color: var(--text);
        }
        .text-muted {
            font-weight: normal;
            color: var(--text-muted);
            font-size: 0.9em;
            margin-top: 4px;
        }
        input[type="range"] {
            width: 100%;
            height: 8px;
            border-radius: 4px;
            outline: none;
            -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: var(--accent);
            cursor: pointer;
        }
        input[type="number"] {
            width: 100%;
            padding: 10px;
            border: 1px solid var(--border);
            border-radius: 8px;
            font-size: 1em;
            background: white;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 52px;
            height: 26px;
            margin-top: 6px;
        }
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: var(--switch-bg);
            transition: .4s;
            border-radius: 26px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: var(--switch-checked);
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        @media (max-width: 600px) {
            .grid {
                grid-template-columns: 1fr;
            }
            .container {
                margin: 10px;
                border-radius: 12px;
            }
        }
        .btn {
            width: 100%;
            padding: 14px;
            background: var(--accent);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 1.1em;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.2s;
        }
        .btn:hover {
            background: #5a6268;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Artificial Sunrise</h1>
        </div>

        <form method="POST" action="/settings">
            <!-- Farbkontrolle -->
            <div class="section">
                <h2 class="section-title">Farbeinstellungen</h2>
                <div class="form-group">
                    <label>Rot</label>
                    <input type="range" name="red" min="0" max="255" value=")"
     << settings.red
     << R"(">
                    <span class="text-muted">Wert: <span id="red-val">)"
     << settings.red
     << R"(</span></span>
                </div>
                <div class="form-group">
                    <label>Grün</label>
                    <input type="range" name="green" min="0" max="255" value=")"
     << settings.green
     << R"(">
                    <span class="text-muted">Wert: <span id="green-val">)"
     << settings.green
     << R"(</span></span>
                </div>
                <div class="form-group">
                    <label>Blau</label>
                    <input type="range" name="blue" min="0" max="255" value=")"
     << settings.blue
     << R"(">
                    <span class="text-muted">Wert: <span id="blue-val">)"
     << settings.blue
     << R"(</span></span>
                </div>
            </div>

            <!-- Alarmkontrolle -->
            <div class="section">
                <h2 class="section-title">Alarmeinstellungen</h2>
                <div class="grid">
                    <div class="form-group">
                        <label>Dauer Sonnenaufgang [min]</label>
                        <input type="number" name="duration_minutes" value=")"
     << settings.duration_minutes
     << R"(" min="1" max="120">
                    </div>
                    <div class="form-group">
                        <label>Dauer Licht nach Sonnenaufgang [min]</label>
                        <input type="number" name="duration_on_brightest" value=")"
     << settings.duration_on_brightest
     << R"(" min="1" max="120">
                    </div>
                    <div class="form-group">
                        <label>Alarm Stunde</label>
                        <input type="number" name="alarm_hour" value=")"
     << settings.alarm_hour
     << R"(" min="0" max="23">
                    </div>
                    <div class="form-group">
                        <label>Alarm Minute</label>
                        <input type="number" name="alarm_minute" value=")"
     << settings.alarm_minute
     << R"(" min="0" max="59">
                    </div>
                </div>

                <div class="form-group">
                    <label>Alarm aktiviert
                        <label class="switch">
                            <input type="checkbox" name="enabled" value="1" )"
     << (settings.enabled ? "checked" : "")
     << R"(>
                            <span class="slider"></span>
                        </label>
                    </label>
                </div>
            </div>

            <div style="padding: 20px;">
                <button type="submit" class="btn">Einstellungen speichern</button>
            </div>
        </form>
    </div>

    <script>
        // Live-Anzeige der RGB-Werte
        ['red', 'green', 'blue'].forEach(name => {
            const slider = document.querySelector(`input[name="${name}"]`);
            const valSpan = document.getElementById(`${name}-val`);
            if (slider && valSpan) {
                slider.oninput = () => valSpan.textContent = slider.value;
            }
        });
    </script>
</body>
</html>
    )";

    std::string response = html.str();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

esp_err_t WebServer::handle_settings_post(httpd_req_t *req)
{
    size_t content_length = req->content_len;

    // Wenn kein Content-Length oder zu groß → Fehler
    if (content_length == 0 || content_length > 4096)
    {
        ESP_LOGW(TAG, "Invalid or missing Content-Length: %u", (unsigned)content_length);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    std::string body;
    body.reserve(content_length);

    char buf[512];
    size_t received = 0;
    while (received < content_length)
    {
        size_t remaining = content_length - received;
        size_t to_read = std::min((size_t)sizeof(buf), remaining);
        int ret = httpd_req_recv(req, buf, to_read);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // Timeout – weiter versuchen
            }
            ESP_LOGE(TAG, "Error receiving body: %d", ret);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        body.append(buf, ret);
        received += ret;
    }

    // Einstellungen unter Mutex aktualisieren
    if (xSemaphoreTake(settings_mutex_, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        parse_params(body, settings_);
        xSemaphoreGive(settings_mutex_);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take mutex for settings update");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Redirect nach Speichern
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

esp_err_t WebServer::handle_settings_get(httpd_req_t *req)
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
         << "\"enabled\":" << (settings.enabled ? "true" : "false")
         << "}";

    httpd_resp_set_type(req, "application/json");
    std::string response = json.str();
    httpd_resp_send(req, response.c_str(), response.length());
    return ESP_OK;
}

// Statische Handler – leiten an Instanz weiter
static esp_err_t root_get_handler(httpd_req_t *req)
{
    return s_instance ? s_instance->handle_root_get(req) : ESP_FAIL;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{
    return s_instance ? s_instance->handle_settings_post(req) : ESP_FAIL;
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    return s_instance ? s_instance->handle_settings_get(req) : ESP_FAIL;
}

esp_err_t WebServer::register_uri_handlers()
{
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