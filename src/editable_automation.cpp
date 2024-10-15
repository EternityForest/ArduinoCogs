#include "editable_automation.h"
#include <string>
#include <vector>
#include <map>
#include <ArduinoJson.h>
#include <stdexcept>
#include <LittleFS.h>

#include "cogs_rules.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "cogs_trouble_codes.h"
#include "cogs_prefs.h"
#include "web/cogs_web.h"
#include "web/generated_data/automation_schema_json_gz.h"

using namespace cogs_rules;

static std::map<std::string, std::shared_ptr<Clockwork>> webClockworks;


static void closeAllClockworks(){

    for (auto it = webClockworks.begin(); it != webClockworks.end(); ++it)
    {
        it->second->close();
    }
    webClockworks.clear();
}

static void badAutomation(){
    closeAllClockworks();
    cogs::addTroubleCode("EBADAUTOMATION");
}

// Creates clockworks according to files.
static void _loadFromFile()
{

    auto f = LittleFS.open("/config/automation.json", "r"); // flawfinder: ignore
    if (!f)
    {
        cogs::logError("No automation.json found.");
        badAutomation();
        return;
    }
    if (f.isDirectory())
    {
        cogs::logError("automation.json is a directory.");
        badAutomation();
        return;
    }

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, f);
    if (error)
    {
        cogs::logError("Error parsing automation.json: " + std::string(error.c_str()));
        badAutomation();
        return;
    }

    JsonVariant vars = doc["vars"];
    if (vars.is<JsonArray>())
    {
        for (auto const &var : vars.as<JsonArray>())
        {
            auto v = cogs_rules::IntTagPoint::getTag(var["name"].as<std::string>(), var["value"].as<int>());
            if(!v){
                cogs::logError("Bad var: " + var["name"].as<std::string>());
                badAutomation();
                return;
            }

            if (var["persistent"].as<bool>()){
                cogs_prefs::addPref(var["name"].as<std::string>());
            }

            v->setValue(var["default"].as<double>() * var["scale"].as<int>());
        }
    }

    JsonVariant clockworks = doc["clockworks"];
    if (!clockworks.is<JsonArray>())
    {
        cogs::logError("No clockworks found.");
        badAutomation();
        return;
    }

    for (auto const &clockworkData : clockworks.as<JsonArray>())
    {

        std::string state = "default";
        unsigned long entered = 1;

        cogs::logInfo("Loading clockwork " + clockworkData["name"].as<std::string>());

        // This clockwork exists but is defined in code, do not overwrite it.
        if (Clockwork::exists(clockworkData["name"].as<std::string>()))
        {
            if (!(webClockworks.count(clockworkData["name"].as<std::string>()) == 1))
            {
                cogs::logError("Clockwork " + clockworkData["name"].as<std::string>() + " is hardcoded.");
                badAutomation();
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
            cogs::logError("Clockwork " + clockworkData["name"].as<std::string>() + " has no states.");
            badAutomation();
            return;
        
            throw std::runtime_error(clockworkData["name"].as<std::string>() + " has no states.");
        }

        for (auto const &stateData : states.as<JsonArray>())
        {
            auto s = new_clockwork->getState(stateData["name"].as<std::string>());

            auto bindings = stateData["bindings"];
            if (!bindings.is<JsonArray>())
            {
                badAutomation();
                return;
            }

            // Now add the bindings
            for (auto const &bindingData : bindings.as<JsonArray>())
            {
                cogs::logInfo("Clockwork " + clockworkData["name"].as<std::string>() + " adding binding " + bindingData["target"].as<std::string>() + " to " + bindingData["source"].as<std::string>());
                auto b = s->addBinding(bindingData["target"].as<std::string>(), bindingData["source"].as<std::string>());
                if(!b->inputExpression){
                    badAutomation();
                    return;
                }

                if (bindingData["mode"].is<const char *>())
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

                if (bindingData["fadeInTime"].is<const char *>())
                {
                    std::string fadeInTime = bindingData["fadeInTime"].as<std::string>();
                    if (!(fadeInTime == "0" || fadeInTime == "0.0" || fadeInTime == ""))
                    {
                        b->fadeInTimeSource = fadeInTime;
                        b->fadeInTime = cogs_rules::compileExpression(fadeInTime);
                        if(!b->fadeInTime){
                            badAutomation();
                            return;
                        }
                        b->trySetupTarget();
                    }
                }

                if (bindingData["alpha"].is<const char *>())
                {
                    std::string alpha = bindingData["alpha"].as<std::string>();
                    if (!(alpha == "1" || alpha == "1.0" || alpha == ""))
                    {
                        b->alphaSource = alpha;
                        b->alpha = cogs_rules::compileExpression(alpha);
                        b->trySetupTarget();
                    }
                }

                if (bindingData["layer"].is<int>())
                {
                    int layer = bindingData["layer"].as<int>();
                    if (layer)
                    {
                        b->layer = layer;
                        b->trySetupTarget();
                    }
                }
                
            }
        }
        // State doesn't exist, use default
        if (!(new_clockwork->states.count(state) == 1))
        {
            state = "default";
            entered = 1;
        }

        cogs::logInfo("Clockwork " + clockworkData["name"].as<std::string>() + " loaded in state " + state + " entered at " + std::to_string(entered));
        new_clockwork->gotoState(state, entered);
    }

    f.close();
    cogs_rules::refreshBindingsEngine();
    cogs::inactivateTroubleCode("EBADAUTOMATION");
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
        doc["tags"][tagPoint->name] = std::string(tagPoint->unit->c_str());
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
        doc["datalist"][tagPoint->name] = std::string(tagPoint->unit->c_str()) +
                                          " " + std::string(tagPoint->description->c_str());
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
void cogs_editable_automation::begin()
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