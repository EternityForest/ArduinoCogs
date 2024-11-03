#include <string>
#include <vector>
#include "cogs_util.h"
#include "web/cogs_web.h"
#include <ArduinoJson.h>
#include "cogs_trouble_codes.h"
#include <LittleFS.h>

std::map<std::string, bool> cogs::troubleCodeStatus;

static void broadcastTroubleCodes()
{
    JsonDocument doc = JsonDocument();

    for (auto const &e : cogs::troubleCodeStatus)
    {
        doc[e.first] = e.second;
    }

    cogs_web::wsBroadcast("__troublecodes__", doc.as<JsonObject>());
}

static void persistTroubleCode(const std::string &code, bool clear)
{

    bool changed = false;

    std::map<std::string, bool> oldStatus;
    // get the old file

    if (!LittleFS.exists("/var/trouble-codes.json"))
    {
        cogs::ensureDirExists("/var");
        File f = LittleFS.open("/var/trouble-codes.json", "w"); // flawfinder: ignore
        f.println("{}");
        f.close();
    }

    File file = LittleFS.open("/var/trouble-codes.json", "r"); // flawfinder: ignore
    if (file)
    {
        JsonDocument doc = JsonDocument();
        deserializeJson(doc, file);
        for (JsonPair pair : doc.as<JsonObject>())
        {
            oldStatus[pair.key().c_str()] = pair.value().as<bool>();
        }
        file.close();
    }

    if (clear)
    {
        if (oldStatus.count(code) == 1)
        {
            changed = true;
            oldStatus.erase(code);
        }
    }
    else
    {
        if (oldStatus.count(code) == 0)
        {
            changed = true;
        }
        oldStatus[code] = true;
    }
    if (!changed)
    {
        return;
    }
    cogs::logInfo("Persisting trouble code: " + code);

    cogs::ensureDirExists("/var");
    JsonDocument doc = JsonDocument();

    for (auto const &e : oldStatus)
    {
        doc[e.first] = e.second;
    }

    File file2 = LittleFS.open("/var/trouble-codes.json", "w"); // flawfinder: ignore
    if (!file2)
    {
        cogs::logError("Error opening trouble codes file");
        return;
    }
    serializeJson(doc, file2);
    file.close();
}

void cogs::addTroubleCode(const std::string &code, bool persist)
{
    bool existed = cogs::troubleCodeStatus.count(code) == 1;
    cogs::troubleCodeStatus[code] = true;
    if (persist)
    {
        persistTroubleCode(code, false);
    }

    if (!existed)
    {
        cogs::logError("New trouble code: " + code);
        broadcastTroubleCodes();
    }
}

void cogs::inactivateTroubleCode(const std::string &code)
{
    if (cogs::troubleCodeStatus.size() == 0)
    {
        return;
    }
    if (cogs::troubleCodeStatus.count(code) == 1)
    {
        if (cogs::troubleCodeStatus[code])
        {
            cogs::troubleCodeStatus[code] = false;
            broadcastTroubleCodes();
        }
    }
}

void cogs::clearTroubleCode(const std::string &code)
{
    if (cogs::troubleCodeStatus.count(code) == 1)
    {
        if (cogs::troubleCodeStatus[code] == false)
        {
            cogs::troubleCodeStatus.erase(code);
        }

        persistTroubleCode(code, true);
        broadcastTroubleCodes();
    }
}

static void clearTroubleCodeWebAPI(AsyncWebServerRequest *request)
{
    if (!request->hasArg("code"))
    {
        request->send(500, "text/plain", "nocodeparam");
        cogs::unlock();
        return;
    }

    std::string c = request->arg("code").c_str();
    cogs::clearTroubleCode(c);
    request->send(200, "text/plain", "ok");
}

static void getTroubleCodesWebAPI(AsyncWebServerRequest *request)
{

    JsonDocument doc = JsonDocument().to<JsonObject>();
    for (auto const &e : cogs::troubleCodeStatus)
    {
        doc[e.first] = e.second;
    }
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void cogs_web::_troubleCodeSetup()
{
    cogs_web::server.on("/api/cogs.trouble-codes", HTTP_GET, getTroubleCodesWebAPI);
    cogs_web::server.on("/api/cogs.clear-trouble-code", HTTP_POST, clearTroubleCodeWebAPI);
}

void cogs_trouble_codes::load()
{
    if (LittleFS.exists("/var/trouble-codes.json"))
    {
        File file = LittleFS.open("/var/trouble-codes.json", "r"); // flawfinder: ignore
        if (file)
        {
            JsonDocument doc = JsonDocument();
            deserializeJson(doc, file);
            for (JsonPair pair : doc.as<JsonObject>())
            {
                cogs::troubleCodeStatus[pair.key().c_str()] = pair.value().as<bool>();
            }
            file.close();
        }
    }
}