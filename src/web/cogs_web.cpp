#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <string.h>

#include "cogs_web.h"
#include "cogs_util.h"
#include "cogs_rules.h"
#include "cogs_global_events.h"
#include "cogs_power_management.h"
#include "web/data/cogs_default_theme.h"

using namespace cogs_web;

AsyncWebServer cogs_web::server(80);

static inline bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

namespace cogs_web
{

    std::vector<NavBarEntry *> navBarEntries;
#include "web/generated_data/page_template_html_gz.h"
#include "web/data/cogs_welcome_page.h"

    bool error_once = false;

    bool mdns_started = false;

    std::string localIp = "";

    unsigned long lastGoodConnection = 0;

    static bool wifiEnabled = true;

    static bool lastWifiEnabled = true;

    static std::shared_ptr<cogs_rules::IntTagPoint> connectedTag = nullptr;

    void handlePSTag(cogs_rules::IntTagPoint *tp)
    {
        if (tp->value[0] == 0)
        {
            WiFi.setSleep(false);
        }
        else
        {
            WiFi.setSleep(true);
        }
    }

    void handleWifiTag(cogs_rules::IntTagPoint *tp)
    {
        if (tp->value[0] == 0)
        {
            wifiEnabled = false;
        }
        else
        {
            wifiEnabled = true;
        }
    }

    void sendGzipFile(AsyncWebServerRequest *request,
                      const uint8_t *data,
                      size_t size,
                      const char *mime)
    {
        AsyncWebServerResponse *response = request->beginResponse_P(200,
                                                                    mime,
                                                                    data,
                                                                    size);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=604800");
        request->send(response);
    }

    void begin()
    {
        cogs_web::setup_cogs_core_web_apis();

        cogs::setDefaultFile("/config/theme.css", std::string(reinterpret_cast<const char *>(nord_theme), sizeof(nord_theme)));

        cogs_web::NavBarEntry::create("Network", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/network.json&filename=/config/network.json");
        cogs_web::NavBarEntry::create("Device", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/device.json&filename=/config/device.json");
        cogs_web::NavBarEntry::create("Files", "/default-template?load-module=/builtin/files_app.js");
        cogs_web::NavBarEntry::create("Dashboard", "/default-template?load-module=/builtin/dashboard_app.js");

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/default-template?load-module=/builtin/welcome_page"); });

        server.on("/default-template", HTTP_GET, [](AsyncWebServerRequest *request)
                  { cogs_web::sendGzipFile(request, page_template_html_gz, sizeof(page_template_html_gz), "text/html"); });

        server.on("/builtin/welcome_page", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "application/javascript", welcome_page); });

        server.begin();
    }

    /// Poll this periodically to check if wifi is connected.
    void check_wifi(bool force = false)
    {

        if (connectedTag.get())
        {
            if (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0"))
            {
                int rssi = WiFi.RSSI();
                connectedTag->setValue(rssi);

                // /// Assume max tx power to get worst case loss
                // int loss = rssi - 30;

                // /// Try to target -70dBm at RX
                // int tx = -70+loss;

                // if(tx<0){ tx=0; }
                // if(tx>30){ tx=30; }

                // WiFi.setTxPower(rssi);
            }
            else
            {
                connectedTag->setValue(-999);
            }
        }

        if (!wifiEnabled)
        {
            if (lastWifiEnabled)
            {

                if ((cogs::uptime() > (60 * 5 * 1000)) || cogs_pm::allowImmediateSleep)
                {
                    cogs::logInfo("WIFI OFF");
                    WiFi.mode(WIFI_OFF);
                    lastWifiEnabled = wifiEnabled;
                    return;
                }
            }
        }

        lastWifiEnabled = wifiEnabled;

        if (!mdns_started)
        {
            if (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0"))
            {
                cogs::logInfo("MDNS started: " + cogs::getHostname());
                if (!MDNS.begin(cogs::getHostname().c_str()))
                {
                    cogs::logError("Error setting up MDNS responder!");
                }
                MDNS.addService("http", "tcp", 80);
                mdns_started = true;
            }
        }

        if (!force)
        {
            if (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0"))
            {
                lastGoodConnection = millis();
            }
            if (millis() - lastGoodConnection < 20000)
            {
                return;
            }
            else
            {
                // Connected but no IP for 20 seconds
                WiFi.mode(WIFI_OFF);
            }
        }

        auto f = LittleFS.open("/config/network.json", "r"); // flawfinder: ignore

        if ((!f) || f.isDirectory())
        {
            if (!error_once)
            {
                cogs::logError("No network.json found.");
                error_once = true;
            }
            return;
        }

        auto s = f.readString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, s);
        if (error)
        {
            if (!error_once)
            {
                cogs::logError("Error parsing network.json");
                Serial.println(error.f_str());

                error_once = true;
            }
            return;
        }

        std::string default_ssid;
        std::string default_pass;
        std::string default_host;

        if (doc["ssid"].is<const char *>())
        {
            default_ssid = doc["ssid"].as<const char *>();
        }

        if (doc["password"].is<const char *>())
        {
            default_pass = doc["password"].as<const char *>();
        }

        if (doc["hostname"].is<const char *>())
        {
            default_host = doc["hostname"].as<const char *>();
        }
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);

        auto slptag = cogs_rules::IntTagPoint::getTag("$wifi.ps", 1, 1);

        WiFi.setSleep(slptag->value[0] > 0);
        WiFi.setHostname(default_host.c_str());

        Serial.println("Connecting to WiFi");
        Serial.println(default_ssid.c_str());
        WiFi.begin(default_ssid.c_str(), default_pass.c_str());
        // Give it 20s to connect
        lastGoodConnection = millis();
    }

    std::string addCacheIDToURL(const std::string &u)
    {
        std::string url = u;
        std::string dt = std::string(__DATE__) + std::string(__TIME__);
        // strip whitespace
        dt.erase(std::remove_if(dt.begin(), dt.end(), isspace), dt.end());

        if (url.find("?") != std::string::npos)
        {
            url = url + "&cacheid=" + dt;
        }
        else
        {
            url = url + "?cacheid=" + dt;
        }

        return url;
    }

    void setDefaultWifi(std::string ssid, std::string password, std::string hostname)
    {
        cogs::setDefaultFile("/config/network.json",
                             "{\"ssid\":\"" + ssid +
                                 "\",\n\"password\":\"" + password +
                                 "\"\n}");
        cogs::setDefaultFile("/config/device.json",
                             "{\"hostname\":\"" + hostname +
                                 "\"\n}");
        cogs_web::check_wifi();
    };

    static void handleEvent(cogs::GlobalEvent event, int n, const std::string &path)
    {
        if (event == cogs::fileChangeEvent)
        {
            if (ends_with(path, "config/network.json"))
            {
                cogs_web::check_wifi(true);
            }
        }
    }

    static void slowPoll()
    {
        check_wifi();
    }

    void manageWifi()
    {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);

        connectedTag = cogs_rules::IntTagPoint::getTag("$wifi.rssi", -999, 1);
        connectedTag->setUnit("dBm");

        auto wtag = cogs_rules::IntTagPoint::getTag("$wifi.on", 1, 1);
        auto pstag = cogs_rules::IntTagPoint::getTag("$wifi.ps", 1, 1);

        pstag->subscribe(&handlePSTag);
        wtag->subscribe(&handleWifiTag);
        lastGoodConnection = millis();
        check_wifi();
        cogs::globalEventHandlers.push_back(&handleEvent);
        cogs::slowPollHandlers.push_back(&slowPoll);
        // cogs::registerFastPollHandler(&doMDNS);
    }
}
