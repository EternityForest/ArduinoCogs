
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <string.h>
#include "esp_random.h"

#include "cogs_web.h"
#include "cogs_static_data.h"

/// Handles requests to /api/cogs.navbar by returning a JSON list of the nav bar entries

using namespace cogs_web;

static void navbar_handler(AsyncWebServerRequest *request)
{
    JsonDocument doc;

    int ctr = 0;
    for (auto e : navBarEntries)
    {
        doc["entries"][ctr]["title"] = e.title;
        doc["entries"][ctr]["url"] = e.url;
    }

    char output[2048];
    serializeJson(doc, output, 2048);

    request->send(200, "application/json", output);
}

static void listdir_handler(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    char resp[32000];

    if (!request->hasParam("dir"))
    {
        request->send(500);
        return;
    }

    auto dir = LittleFS.open(request->getParam("dir")->value(), "r");
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
    request->send(LittleFS, request->getParam("file")->value().c_str());
}

static void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{

    std::string redirect = "";
    if (request->hasParam("redirect"))
    {
        redirect = request->getParam("redirect")->value().c_str();
    }

    if (!index)
    {
        // open the file on first call and store the file handle in the request object
        request->_tempFile = LittleFS.open(filename, "w");
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
        request->redirect("/");
    }
}

namespace cogs_web
{

    void setup_cogs_core_web_apis()
    {
        server.on("/builtin/lit.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                  {
                      AsyncWebServerResponse *response = request->beginResponse(200, 
                      "application/javascript",
                      cogs_builtin_static::lit_min_js, 
                      sizeof(cogs_builtin_static::lit_min_js));

                      response->addHeader("Content-Encoding", "gzip");
                      request->send(response); });

        server.on("/api/cogs.navbar", HTTP_GET, navbar_handler);
        server.on("/api/cogs.listdir", HTTP_GET, listdir_handler);

        server.on("/api/cogs.upload", HTTP_POST, [](AsyncWebServerRequest *request)
                  { request->send(200); }, handleUpload);

        server.on("/api/cogs.download", HTTP_GET, handleDownload);
    }
}