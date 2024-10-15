#include "cogs_prefs.h"
#include "cogs_global_events.h"
#include "cogs_reggshell.h"
#include <ArduinoJson.h>

namespace cogs_prefs
{
    bool started = false;

    unsigned long lastFlush = 0;
    bool dirty = false;

    /// Have to store in double since we don't know resolution ahead of time
    std::map<std::string, double> prefs;

    void slowPollHandler()
    {
        if (dirty)
        {
            if (millis() - lastFlush > 60000)
            {
                lastFlush = millis();
                flushPrefs();
            }
        }
    }

    static void onPrefsTagSet(cogs_rules::IntTagPoint *tp)
    {
        dirty = true;
        double v = tp->value[0];
        v /= tp->scale;
        prefs[tp->name] = v;
        slowPollHandler();
    }

    static void loadPrefs()
    {
        if (!LittleFS.exists("/var/prefs.json"))
        {
            return;
        }

        File f = LittleFS.open("/var/prefs.json", "r");

        if (!f)
        {
            return;
        }
        JsonDocument doc = JsonDocument();

        // Corrupt file goes away
        if (deserializeJson(doc, f))
        {
            LittleFS.remove("/var/prefs.json");
            return;
        }

        for (JsonPair pair : doc.as<JsonObject>())
        {
            addPref(pair.key().c_str());
        }
        f.close();
    }

    static void handleGlobalEvent(cogs::GlobalEvent e, int v, const std::string &s)
    {
        if (e == cogs::fileChangeEvent)
        {
            if (s == "/var/prefs.json")
            {
                begin();
            }
        }
    }


    static void flushCmd(reggshell::Reggshell *rs, const char *arg1, const char *arg2, const char *arg3){
        flushPrefs();
        rs->println("Prefs flushed");
    }

    void begin()
    {
        if (started)
        {
            return;
        }
        started = true;
        loadPrefs();
        cogs::slowPollHandlers.push_back(slowPollHandler);
        cogs::globalEventHandlers.push_back(handleGlobalEvent);
        cogs_reggshell::interpreter->addSimpleCommand("save-prefs", &flushCmd, "Save all prefs to disk");
    }

    void addPref(const std::string &name)
    {
        if (!started)
        {
            cogs::logError("Prefs not enabled");
            return;
        }
        // if tag not in prefs
        if (prefs.find(name) == prefs.end())
        {
            if (cogs_rules::IntTagPoint::exists(name))
            {
                auto tp = cogs_rules::IntTagPoint::getTag(name, 0);
                prefs[name] = (double)tp->value[0]/(double)tp->scale;
                tp->subscribe(onPrefsTagSet);
            }
        }
        else{
            auto tp = cogs_rules::IntTagPoint::getTag(name, 0);
            if(tp){
                tp->setValue(prefs[name]*tp->scale);
            }
        }
    }

    void flushPrefs()
    {
        if (!dirty)
        {
            return;
        }

        cogs::logInfo("Flushing prefs");
        dirty = false;
        cogs::ensureDirExists("/var");
        File f = LittleFS.open("/var/prefs.json", "w");
        JsonDocument doc = JsonDocument();
        for (auto const &pair : prefs)
        {
            doc[pair.first.c_str()] = pair.second;
        }
        serializeJson(doc, f);
        f.close();
    }
}