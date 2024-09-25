
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
#include "cogs_bindings_engine.h"

namespace cogs_web
{
    //! The global ESPAsyncWebServer object.
    /// Everything adds it's page routes here.
    extern AsyncWebServer server;

    class NavBarEntry;

    extern std::list<cogs_web::NavBarEntry *> navBarEntries;

    //! Represents one entry in the nav bar.  On creation, it is automatically active.
    //! Used by plugins.  Created with cogs_web::NavBarEntry::create(title, url)
    class NavBarEntry
    {
    private:
        // Private so nobody puts one on the stack and then has it vanish.
        NavBarEntry(std::string title, std::string url)
        {
            this->title = title;
            this->url = url;
        };

    public:
        // Title may be changed after instantiation
        std::string title;

        // Url may be changed after instantiation
        std::string url;

        /// Create a new nav bar entry.  Entries are automatically added to navBarEntries.
        /// Returns a pointer to the new entry.
        static NavBarEntry *create(std::string title, std::string url)
        {
            NavBarEntry *entry = new NavBarEntry(title, url);
            navBarEntries.push_back(entry);
            return entry;
        }
    };

    //! Enable managing the WiFi based on the config file
    void manageWifi();

    //! Set up and enable the web UI for cogs.
    void setupWebServer();

    //! Used to set the default wifi settings.
    /// /config/network.json can override these settings with the corresponding keys.
    void setDefaultWifi(std::string ssid, std::string password, std::string hostname);

    //! Make a json document with this key and val and push it everywhere.
    void wsBroadcast(const char *key, const char *val);

    //! Push all tag point changes to web UI
    void exposeTagPoint(std::shared_ptr<cogs_rules::IntTagPoint>);

    /// Private
    void setup_cogs_core_web_apis();
    // Called internally
    void setupWebSocketServer();

}
