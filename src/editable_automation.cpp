#include "editable_automation.h"
#include <string>
#include <list>
#include <map>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <stdexcept>

#include "cogs_bindings_engine.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "web/cogs_web.h"
#include "web/data/cogs_automation_schema.h"



using namespace cogs_rules;

static std::map<std::string, std::shared_ptr<Clockwork>> webClockworks;

// Creates clockworks according to files.
static void _loadFromFile()
{
    auto f = LittleFS.open("/config/automation.json", "r"); // flawfinder: ignore
    if (!f)
    {
        cogs::logError("No automation.json found.");
        return;
    }
    if (f.isDirectory())
    {
        throw std::runtime_error("automation.json is a directory.");
    }

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, f);
    if (error)
    {
        throw std::runtime_error(error.c_str());
    }

    JsonVariant clockworks = doc["clockworks"];
    if (!clockworks.is<JsonArray>())
    {
        throw std::runtime_error("No clockworks in automation.json");
    }

    for (auto clockworkData : clockworks.as<JsonArray>())
    {

        std::string state = "default";
        unsigned long entered = 1;

        cogs::logInfo("Loading clockwork " + clockworkData["name"].as<std::string>());

        // This clockwork exists but is defined in code, do not overwrite it.
        if (Clockwork::exists(clockworkData["name"].as<std::string>()))
        {
            if (!webClockworks.contains(clockworkData["name"].as<std::string>()))
            {
                cogs::logError("Clockwork " + clockworkData["name"].as<std::string>() + " is hardcoded.");
                continue;
            }
            else
            {
                // It exists but is not defined in code, copy over state and remake
                if (webClockworks[clockworkData["name"].as<std::string>()]->currentState)
                {
                    cogs::logInfo("Keeping old state for clockwork " +
                                  clockworkData["name"].as<std::string>() + ": " +
                                  webClockworks[clockworkData["name"].as<std::string>()]->currentState->name);

                    state = webClockworks[clockworkData["name"].as<std::string>()]->currentState->name;
                }
                entered = webClockworks[clockworkData["name"].as<std::string>()]->enteredAt;
                webClockworks[clockworkData["name"].as<std::string>()]->close();
            }
        }

        auto new_clockwork = Clockwork::getClockwork(clockworkData["name"].as<std::string>());

        webClockworks[clockworkData["name"].as<std::string>()] = new_clockwork;
        
        JsonVariant states = clockworkData["states"];
        if (!states.is<JsonArray>())
        {
            throw std::runtime_error(clockworkData["name"].as<std::string>() + " has no states.");
        }

        for (auto stateData : states.as<JsonArray>())
        {
            auto s = new_clockwork->getState(stateData["name"].as<std::string>());

            auto bindings = stateData["bindings"];
            if (!bindings.is<JsonArray>())
            {
                throw std::runtime_error("Clockwork " + clockworkData["name"].as<std::string>() + " bindings type error");
            }

            // Now add the bindings
            for (auto bindingData : bindings.as<JsonArray>())
            {
                cogs::logInfo("Clockwork " + clockworkData["name"].as<std::string>() + " adding binding " + bindingData["target"].as<std::string>() + " to " + bindingData["source"].as<std::string>());
                s->addBinding(bindingData["target"].as<std::string>(), bindingData["source"].as<std::string>());
            }
        }
        // State doesn't exist, use default
        if (!new_clockwork->states.contains(state))
        {
            state = "default";
            entered = 1;
        }

        cogs::logInfo("Clockwork " + clockworkData["name"].as<std::string>() + " loaded in state " + state + " entered at " + std::to_string(entered));
        new_clockwork->gotoState(state, entered);
    }

    f.close();
    cogs_rules::refreshBindingsEngine();
}

// Wrap it in an exception handler
static void loadFromFile(){
    try{
        _loadFromFile();
    }catch(std::exception &e){
        cogs::logError(e.what());
    }
}

static void fileChangeHandler(cogs::GlobalEvent evt, int dummy, const std::string & filename)
{
    if (evt == cogs::fileChangeEvent)
    {
        if (filename == "/config/automation.json")
        {
            loadFromFile();
        }
    }
}

void cogs_editable_automation::setupEditableAutomation()
{
    loadFromFile();

    cogs_web::server.on("/builtin/schemas/automation.json", HTTP_GET, [](AsyncWebServerRequest *request)
                    { request->send(200, "application/json", cogs_automation_schema); });

    // Add a navbar entry allowing editing of automation.json
    cogs_web::NavBarEntry::create("Automation Rules", 
    "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/automation.json&filename=/config/automation.json");
    
    cogs::globalEventHandlers.push_back(fileChangeHandler);
}