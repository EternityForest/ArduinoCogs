#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>
#include "esp_random.h"

#include "cogs_web.h"

using namespace cogs_web;


AsyncWebServer cogs_web::server(80);

namespace cogs_web
{

    std::list<NavBarEntry> navBarEntries;

    static const char welcome_page[] = R"(
<script type="module">
    import { html, css, LitElement } from '/builtin/lit.min.js';

    export class SimpleGreeting extends LitElement {
        static properties = {
            name : {type : String},
        };

        static styles = css`p
        {
        color:
            blue
        }
        `;

        constructor()
        {
            super();
            this.name = 'Somebody';
            this.timer = setInterval(() => {
                this.name = Date.now();
            },1000);
        }

        disconnectedCallback()
        {
            clearInterval(this.timer);
            super.disconnectedCallback();
        }

        render()
        {
            return html`<p> Hello, ${ this.name }
            !</ p>`;
        }
    }

    customElements.define('simple-greeting', SimpleGreeting);

</script>

<simple-greeting></simple-greeting>

<div id="app"></div>
)";

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
