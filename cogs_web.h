
/**
 * @file
 * 
 * @section api Builtin APIs
 * 
 * @subsection /api/cogs.navbar
 * Return a JSON list of navbar entries. 
 * Data will be at json['entries'][i]['title'] and json['entries'][i]['url']
 * 
 * @subsection /api/cogs.upload
 * Upload a file here with a filename. param.  If the file exists, it will be overwritten.
 * If redirect is specified, it will be used as the redirect URL
 * 
 * @subsection /api/cogs.listdir
 * List the contents of a directory given by the 'dir' param.
 */

#pragma once
#include <string>
#include <list>
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

namespace cogs_web
{
    //! The global ESPAsyncWebServer object.
    /// Everything adds it's page routes here.
    extern AsyncWebServer server;

    //! Represents one entry in the nav bar.
    //! Used by plugins.
    class NavBarEntry
    {
    public:
        std::string title;
        std::string url;
        NavBarEntry(std::string title, std::string url)
        {
            this->title = title;
            this->url = url;
        };
    };

    //! List of all nav bar entries.
    /// After creating a new entry, add it here with navBarEntries.push_back(entry);
    extern std::list<cogs_web::NavBarEntry>  navBarEntries;

    //! Set up and enable the web UI for cogs.
    void setupWebServer();

    //! Used to set the default wifi settings.
    /// /config/network.json can override these settings with the corresponding keys.
    void setDefaultWifi(std::string ssid, std::string password, std::string hostname);

    /// Private
    void setup_cogs_core_web_apis();

}
