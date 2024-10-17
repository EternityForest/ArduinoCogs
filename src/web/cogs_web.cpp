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
static std::map<std::string, unsigned long> wifiFailTimestamps;
static std::string connectingAttempt = "";

static unsigned long lastWifiScan = 0;

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

        cogs_web::NavBarEntry::create("🛜Network", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/network.json&filename=/config/network.json");
        cogs_web::NavBarEntry::create("🛠️Device", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/device.json&filename=/config/device.json");
        cogs_web::NavBarEntry::create("📂Files", "/default-template?load-module=/builtin/files_app.js");
        cogs_web::NavBarEntry::create("🎚️Dashboard", "/default-template?load-module=/builtin/dashboard_app.js");

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/default-template?load-module=/builtin/welcome_page"); });

        server.on("/default-template", HTTP_GET, [](AsyncWebServerRequest *request)
                  { cogs_web::sendGzipFile(request, page_template_html_gz, sizeof(page_template_html_gz), "text/html"); });

        server.on("/builtin/welcome_page", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "application/javascript", welcome_page); });

        server.begin();
    }

    static int rssiForScannedNetwork(const std::string &ssid)
    {
        int num = WiFi.scanComplete();
        if (num <= 0)
        {
            return -127;
        }

        for (int i = 0; i < num; i++)
        {
            if (strcmp(WiFi.SSID(i).c_str(), ssid.c_str()) == 0)
            {
                return WiFi.RSSI(i);
            }
        }
        return -127;
    }
    /// Poll this periodically to check if wifi is connected.
    void check_wifi(bool force = false)
    {

        if (connectedTag.get())
        {
            if (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0"))
            {
                int rssi = WiFi.RSSI();
                /// Update every 240 seconds or when the val changes by 4.
                connectedTag->smartSetValue(rssi, 4, 240000);

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

        if (WiFi.status() == WL_CONNECTED)
        {
            if (wifiFailTimestamps.count(connectingAttempt) > 0)
            {
                wifiFailTimestamps.erase(connectingAttempt);
            }
            connectingAttempt = "";
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
            if (lastGoodConnection > 0)
            {
                if (millis() - lastGoodConnection < 20000)
                {
                    return;
                }
                else
                {
                    // Connected but no IP for 20 seconds
                    WiFi.mode(WIFI_OFF);
                    WiFi.mode(WIFI_STA);
                }
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

        if ((!lastWifiScan) || (millis() - lastWifiScan > 40000))
        {
            cogs::logInfo("Begin wifi scan");
            WiFi.scanNetworks(true);
            lastWifiScan = millis();
        }
        if (WiFi.status() == WL_CONNECT_FAILED)
        {
            cogs::logError("Wifi connect failed");
            if (connectingAttempt.length() > 0)
            {
                wifiFailTimestamps[connectingAttempt] = millis();
                connectingAttempt = "";
            }
        }

        if (WiFi.scanComplete() < 0)
        {
            return;
        }
        else
        {
            cogs::logInfo("j");
        }

        auto default_host = cogs::getHostname();

        // Find the first matching WiFi network in our list.
        auto networks = doc["networks"].as<JsonArray>();
        for (auto network : networks)
        {
            std::string ssid = network["ssid"].as<std::string>();
            std::string pass = network["password"].as<std::string>();
            int minRSSI = -100;

            cogs::logInfo("Wifi candidate:");
            cogs::logInfo(ssid);

            if (rssiForScannedNetwork(ssid) > minRSSI)
            {

                if (wifiFailTimestamps.count(ssid) > 0)
                {
                    if (millis() - wifiFailTimestamps[ssid] > 30000)
                    {
                        cogs::logInfo("Skipping");
                        continue;
                    }
                }

                connectingAttempt = ssid;

                WiFi.begin(ssid.c_str(), pass.c_str());
                auto slptag = cogs_rules::IntTagPoint::getTag("$wifi.ps", 1, 1);

                WiFi.setSleep(slptag->value[0] > 0);
                WiFi.setHostname(default_host.c_str());

                Serial.println("Connecting to WiFi");
                Serial.println(ssid.c_str());
                // Give it 20s to connect
                lastGoodConnection = millis();

                break;
            }
        }
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
                             "{\"networks\":[{\"ssid\":\"" + ssid +
                                 "\",\n\"password\":\"" + password +
                                 "\"\n}]}");
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
        cogs::logInfo("Cogs is managing wifi");
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);

        connectedTag = cogs_rules::IntTagPoint::getTag("$wifi.rssi", -999, 1);
        connectedTag->setUnit("dBm");

        auto wtag = cogs_rules::IntTagPoint::getTag("$wifi.on", 1, 1);
        auto pstag = cogs_rules::IntTagPoint::getTag("$wifi.ps", 1, 1);

        pstag->subscribe(&handlePSTag);
        wtag->subscribe(&handleWifiTag);
        check_wifi();
        cogs::globalEventHandlers.push_back(&handleEvent);
        cogs::slowPollHandlers.push_back(&slowPoll);
        // cogs::registerFastPollHandler(&doMDNS);
    }
}
