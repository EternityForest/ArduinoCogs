
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>
#include "esp_random.h"

#include "web/cogs_web.h"
#include "web/cogs_static_data.h"
#include "cogs_util.h"
#include "cogs_global_events.h"

/// Handles requests to /api/cogs.navbar by returning a JSON list of the nav bar entries

using namespace cogs_web;

static void navbar_handler(AsyncWebServerRequest *request)
{
    JsonDocument doc;

    int ctr = 0;
    for (auto e : navBarEntries)
    {
        doc["entries"][ctr]["title"] = e->title;
        doc["entries"][ctr]["url"] = e->url;
    }

    char output[2048];
    serializeJson(doc, output, 2048);

    request->send(200, "application/json", output);
}

static void listdir_handler(AsyncWebServerRequest *request)
{
    int args = request->args();
    for (int i = 0; i < args; i++)
    {
        Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    }

    JsonDocument doc;
    char resp[32000];

    if (!request->hasArg("dir"))
    {
        request->send(500);
        return;
    }

    auto dir = LittleFS.open(request->arg("dir").c_str(), "r");
    if (!dir)
    {
        request->send(404);
        return;
    }
    if (!dir.isDirectory())
    {
        request->send(404);
        return;
    }

    File f = dir.openNextFile();

    while (f)
    {

        doc["files"][dir.name()]["size"] = f.size();

        if (f.isDirectory())
        {
            doc["files"][dir.name()]["type"] = "dir";
        }
        else
        {
            doc["files"][dir.name()]["type"] = "file";
        }

        f = dir.openNextFile();
    }

    serializeJson(doc, resp, 32000);
    request->send(200, "application/json", resp);
}

static void handleDownload(AsyncWebServerRequest *request)
{

    int args = request->args();
    for (int i = 0; i < args; i++)
    {
        Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    }

    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        return;
    }

    request->send(LittleFS, request->arg("file").c_str());
}

static void handleUpload(AsyncWebServerRequest *request, String orig_filename, size_t index, uint8_t *data, size_t len, bool final)
{

    int args = request->args();
    for (int i = 0; i < args; i++)
    {
        Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());
    }

    std::string redirect = "/";
    if (request->hasArg("redirect"))
    {
        redirect = request->arg("redirect").c_str();
    }

    std::string path;

    if (request->hasArg("path"))
    {
        path = request->arg("path").c_str();

        // Allow auto using the filename
        if (request->arg("path").endsWith("/"))
        {
            path = path + orig_filename.c_str();
        };
    }
    else
    {
        request->send(500, "text/plain", "nofnparam");
    }

    if (!index)
    {
        // open the file on first call and store the file handle in the request object
        request->_tempFile = LittleFS.open(path.c_str(), "w");
    }

    if (!request->_tempFile)
    {
        request->send(500, "text/plain", "cantopenfile");
    }

    if (len)
    {
        // stream the incoming chunk to the opened file
        request->_tempFile.write(data, len);
    }

    if (final)
    {
        // close the file handle as the upload is now done
        request->_tempFile.close();
        cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, path);

        request->redirect(redirect.c_str());
    }
}

static void handleSetFile(AsyncWebServerRequest *request)
{
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        return;
    }

    if (!request->hasArg("data"))
    {
        request->send(500, "text/plain", "nodata");
        return;
    }
    auto f = LittleFS.open(request->arg("file").c_str(), "w");
    if (!f)
    {
        request->send(500, "text/plain", "cantopenfile");
        return;
    }

    f.print(request->arg("data").c_str());
    f.close();
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);
}

namespace cogs_web
{

    void setup_cogs_core_web_apis()
    {
        setup_builtin_static();

        server.on("/api/cogs.navbar", HTTP_GET, navbar_handler);
        server.on("/api/cogs.listdir", HTTP_GET, listdir_handler);

        server.on("/api/cogs.upload", HTTP_POST, [](AsyncWebServerRequest *request)
                  { request->send(200); }, handleUpload);

        server.on("/api/cogs.setfile", HTTP_POST, handleSetFile);

        server.on("/api/cogs.download", HTTP_GET, handleDownload);
    }
}