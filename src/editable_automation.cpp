#include "editable_automation.h"
#include <string>
#include <vector>
#include <map>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <stdexcept>

#include "cogs_bindings_engine.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "web/cogs_web.h"
#include "web/generated_data/automation_schema_json_gz.h"

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

    for (auto const & clockworkData : clockworks.as<JsonArray>())
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

        for (auto const & stateData : states.as<JsonArray>())
        {
            auto s = new_clockwork->getState(stateData["name"].as<std::string>());

            auto bindings = stateData["bindings"];
            if (!bindings.is<JsonArray>())
            {
                throw std::runtime_error("Clockwork " + clockworkData["name"].as<std::string>() + " bindings type error");
            }

            // Now add the bindings
            for (auto const & bindingData : bindings.as<JsonArray>())
            {
                cogs::logInfo("Clockwork " + clockworkData["name"].as<std::string>() + " adding binding " + bindingData["target"].as<std::string>() + " to " + bindingData["source"].as<std::string>());
                auto b = s->addBinding(bindingData["target"].as<std::string>(), bindingData["source"].as<std::string>());

                if (bindingData.containsKey("mode"))
                {
                    std::string mode = bindingData["mode"].as<std::string>();
                    if (mode == "onchange")
                    {
                        b->onchange = true;
                    }
                    if (mode == "onenter")
                    {
                        b->onenter = true;
                        b->freeze = true;
                    }
                    if (mode == "onframe")
                    {
                        b->onchange = false;
                        b->freeze = false;
                    }
                }
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
static void loadFromFile()
{
    try
    {
        _loadFromFile();
    }
    catch (std::exception &e)
    {
        cogs::logError(e.what());
    }
}

static void fileChangeHandler(cogs::GlobalEvent evt, int dummy, const std::string &filename)
{
    if (evt == cogs::fileChangeEvent)
    {
        if (filename == "/config/automation.json")
        {
            loadFromFile();
        }
    }
}

static void listTargetsApi(AsyncWebServerRequest *request)
{
    JsonDocument doc;

    // Add every tag point name to the enum property
    for (auto const &tagPoint : cogs_rules::IntTagPoint::all_tags)
    {
        doc["tags"][tagPoint.first] = tagPoint.second->unit + " " + tagPoint.second->description;
    }
    char *buf = reinterpret_cast<char *>(malloc(8192));
    if (!buf)
    {
        request->send(500);
        return;
    }
    serializeJson(doc, buf, 8192);
    request->send(200, "application/json", buf);
    free(buf);
}

static void exprDatalist(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    for (auto const &tagPoint : cogs_rules::IntTagPoint::all_tags)
    {
        doc["datalist"][tagPoint.first] = tagPoint.second->unit + " " + tagPoint.second->description;
    }
    for (auto const &f : cogs_rules::user_functions0)
    {
        doc["datalist"][f.first + "()"] = "";
    }
    for (auto const &f : cogs_rules::user_functions1)
    {
        doc["datalist"][f.first + "(x)"] = "";
    }
    for (auto const &f : cogs_rules::user_functions2)
    {
        doc["datalist"][f.first + "(a,b)"] = "";
    }
    for (auto const &f : cogs_rules::constants)
    {
        doc["datalist"][f.first] = std::to_string(*f.second);
    }
    char *buf = reinterpret_cast<char *>(malloc(8192));
    if (!buf)
    {
        request->send(500);
        return;
    }
    serializeJson(doc, buf, 8192);
    request->send(200, "application/json", buf);
    free(buf);
}
void cogs_editable_automation::setupEditableAutomation()
{
    loadFromFile();

    cogs_web::server.on("/api/tags", HTTP_GET, listTargetsApi);
    cogs_web::server.on("/api/expr", HTTP_GET, exprDatalist);
    cogs_web::server.on("/builtin/schemas/automation.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, automation_schema_json_gz, sizeof(automation_schema_json_gz), "application/json"); });

    // Add a navbar entry allowing editing of automation.json
    cogs_web::NavBarEntry::create("Automation Rules",
                                  "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/automation.json&filename=/config/automation.json");

    cogs::globalEventHandlers.push_back(fileChangeHandler);
}