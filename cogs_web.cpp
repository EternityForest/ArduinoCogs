#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>
#include "esp_random.h"

#include "cogs_web.h"
#include "cogs_util.h"

using namespace cogs_web;


AsyncWebServer cogs_web::server(80);

namespace cogs_web
{

    std::list<NavBarEntry *> navBarEntries;
    #include "webdata/cogs_welcome_page.h"

    static std::string default_ssid = "";
    static std::string default_pass = "";
    static std::string default_host = "";

    void setupWebServer()
    {
        cogs_web::setup_cogs_core_web_apis();

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/html", welcome_page); });
        server.begin();
    }

    /// Poll this periodically to check if wifi is connected.
    void check_wifi()
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            return;
        }

        auto f = LittleFS.open("/config/network.json", "r");

        bool use_cfg_wifi = true;

        if (!f || f.isDirectory())
        {
            use_cfg_wifi = false;
        }

        auto s = f.readString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, s);
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            use_cfg_wifi = false;
        }

        if (use_cfg_wifi)
        {
            if (doc.containsKey("ssid"))
            {
                default_ssid = doc["ssid"].as<const char *>();
            }

            if (doc.containsKey("password"))
            {
                default_pass = doc["password"].as<const char *>();
            }

            if (doc.containsKey("hostname"))
            {
                default_host = doc["hostname"].as<const char *>();
            }
        }

        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(default_host.c_str());

        Serial.println("Connecting to WiFi");
        Serial.println(default_ssid.c_str());
        Serial.println(default_pass.c_str());
        WiFi.begin(default_ssid.c_str(), default_pass.c_str());
    }

    void setDefaultWifi(std::string ssid, std::string password, std::string hostname)
    {
        default_host = hostname;
        default_ssid = ssid;
        default_pass = password;
        cogs_web::check_wifi();
    };

}
