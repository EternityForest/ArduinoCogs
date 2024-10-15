#include <map>
#include <string>
#include <LittleFS.h>

#include "cogs_rules.h"

namespace cogs_prefs{
    extern unsigned long lastFlush;
    extern bool dirty;
    extern std::map<std::string, double> prefs;
    extern bool started;

    void begin();

    /// Mark tag as a pref.  Must call after begin() or it does nothing
    void addPref(const std::string &name);
    void flushPrefs();
}