#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>

#include "cogs_web.h"
#include "cogs_util.h"
#include "cogs_global_events.h"

using namespace cogs_web;

AsyncWebServer cogs_web::server(80);

namespace cogs_web
{

    std::list<NavBarEntry *> navBarEntries;
#include "web/data/cogs_page_template.h"
#include "web/data/cogs_welcome_page.h"

    bool error_once = false;

    void setupWebServer()
    {
        cogs_web::setup_cogs_core_web_apis();

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/default-template?load-module=/builtin/welcome_page"); });

        server.on("/default-template", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/html", cogs_page_template); });

        server.on("/builtin/welcome_page", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "application/javascript", welcome_page); });

        server.begin();
    }

    /// Poll this periodically to check if wifi is connected.
    void check_wifi(bool force = false)
    {
        if (!force)
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                return;
            }
        }

        auto f = LittleFS.open("/config/network.json", "r");

        if (!f || f.isDirectory())
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

        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(default_host.c_str());

        Serial.println("Connecting to WiFi");
        Serial.println(default_ssid.c_str());
        WiFi.begin(default_ssid.c_str(), default_pass.c_str());
    }

    void setDefaultWifi(std::string ssid, std::string password, std::string hostname)
    {
        cogs::setDefaultFile("/config/network.json",
                             "{\"ssid\":\"" + ssid +
                                 "\",\n\"password\":\"" + password +
                                 "\",\n\"hostname\":\"" + hostname +
                                 "\"\n}");
        cogs_web::check_wifi();
    };

    static void handleEvent(enum GlobalEvents event, int n, std::string path)
    {
        if (event == cogs::fileChangeEvent)
        {
            if (path.ends_with("config/network.json"))
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
        check_wifi();
        cogs::globalEventHandlers.push_back(&handleEvent);
        cogs::slowPollHandlers.push_back(&slowPoll);
    }

}
