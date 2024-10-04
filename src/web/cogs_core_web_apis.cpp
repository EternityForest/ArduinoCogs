
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
#include "cogs_bindings_engine.h"

/// Handles requests to /api/cogs.navbar by returning a JSON list of the nav bar entries

using namespace cogs_web;

static void navbar_handler(AsyncWebServerRequest *request)
{
    cogs::lock();
    JsonDocument doc;

    int ctr = 0;
    for (auto const & e : navBarEntries)
    {
        doc["entries"][ctr]["title"] = e->title;
        doc["entries"][ctr]["url"] = e->url;
        ctr++;
    }

    char output[4096];
    serializeJson(doc, output, 4096);

    request->send(200, "application/json", output);

    cogs::unlock();
}

static void listdir_handler(AsyncWebServerRequest *request)
{
    cogs::lock();

    JsonDocument doc;

    doc["freeflash"] = LittleFS.totalBytes() - LittleFS.usedBytes();
    doc["totalflash"] = LittleFS.totalBytes();

    char resp[4096];

    if (!request->hasArg("dir"))
    {
        request->send(500);
        goto done;
    }

    auto dir = LittleFS.open(request->arg("dir").c_str(), "r");
    if (!dir)
    {
        request->send(404);
        goto done;
    }
    if (!dir.isDirectory())
    {
        request->send(404);
        goto done;
    }

    File f = dir.openNextFile();

    // Hang onto refs because json lib might not copy
    std::vector<std::string> strs;

    while (f)
    {
        if (f.isDirectory())
        {
            auto s = std::string(f.name());
            strs.push_back(s);
            doc["dirs"][s] = "";

        }
        else
        {
            auto s = std::string(f.name());
            strs.push_back(s);
            doc["files"][s] = f.size();
        }

        f = dir.openNextFile();
    }

    serializeJson(doc, resp, 4096);
    request->send(200, "application/json", resp);

    done:
      cogs::unlock();
}

static void handleDownload(AsyncWebServerRequest *request)
{
    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        goto done;
    }

    request->send(LittleFS, request->arg("file").c_str());

    done:
      cogs::unlock();
}

static void handleUpload(AsyncWebServerRequest *request, String orig_filename, size_t index, uint8_t *data, size_t len, bool final)
{
    cogs::lock();

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

    cogs::unlock();
}

static void handleSetFile(AsyncWebServerRequest *request)
{
    cogs::lock()
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        goto done;
    }

    if (!request->hasArg("data"))
    {
        request->send(500, "text/plain", "nodata");
        goto done;
    }
    auto f = LittleFS.open(request->arg("file").c_str(), "w");
    if (!f)
    {
        request->send(500, "text/plain", "cantopenfile");
        goto done;
    }

    f.print(request->arg("data").c_str());
    f.close();
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);

    done:
      cogs::unlock();
}

static void handleDeleteFile(AsyncWebServerRequest *request){

    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        return;
    }

    LittleFS.remove(request->arg("file").c_str());
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);

    cogs::unlock();
}

static void handleRenameFile(AsyncWebServerRequest *request){

    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        goto done;
    } 
    if (!request->hasArg("newname"))
    {
        request->send(500, "text/plain", "nofileparam");
        goto done;
    }
    
    std::string fn = request->arg("newname").c_str();
    std::string dirname = fn.substr(0, fn.rfind('/'));
    cogs::ensureDirExists(dirname);

    LittleFS.rename(request->arg("file").c_str(), request->arg("newname").c_str());
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);

    done:
      cogs::unlock();
}

static void handleGetTagInfo(AsyncWebServerRequest *request)
{

    cogs::lock();
    if (!request->hasArg("tag"))
    {
        request->send(500, "text/plain", "nofileparam");
        goto done;
    }
    std::string tagname = request->arg("tag").c_str();
    if (!cogs_rules::IntTagPoint::exists(tagname))
    {
        request->send(500, "text/plain", "tagnotfound");
        goto done;
    }
    auto tag = cogs_rules::IntTagPoint::getTag(tagname,0,1);

    JsonDocument doc;
    doc["max"] = tag->max;
    doc["min"] = tag->min;
    doc["unit"] = tag->unit;
    doc["step"] = 1.0/float(tag->scale);
    doc["hi"] = tag->hi;
    doc["lo"] = tag->lo;
    doc["scale"] = tag->scale;
    doc["firstValue"] = tag->value[0];
    doc["length"] = tag->count;

    char buf[256];
    serializeJson(doc, buf, 256);

    request->send(200, "text/plain", buf);

    done:
      cogs::unlock();
}

static void listTags(AsyncWebServerRequest *request){
    cogs::lock();
    JsonDocument doc = JsonDocument();
    for(auto const & tagPoint : cogs_rules::IntTagPoint::all_tags){
        doc["tags"][tagPoint->name] = tagPoint->value[0];        
    }
    
    char buf[2048];
    serializeJson(doc, buf, 2048);

    request->send(200, "text/plain", buf);

    cogs::unlock();
}


namespace cogs_web
{

    void setup_cogs_core_web_apis()
    {
        setup_builtin_static();
        setupWebSocketServer();

        server.on("/api/cogs.navbar", HTTP_GET, navbar_handler);
        server.on("/api/cogs.listdir", HTTP_GET, listdir_handler);

        server.on("/api/cogs.upload", HTTP_POST, [](AsyncWebServerRequest *request)
                  { request->send(200); }, handleUpload);

        server.on("/api/cogs.setfile", HTTP_POST, handleSetFile);

        server.on("/api/cogs.download", HTTP_GET, handleDownload);

        server.on("/api/cogs.deletefile", HTTP_POST, handleDeleteFile);
        server.on("/api/cogs.renamefile", HTTP_POST, handleRenameFile);

        server.on("/api/cogs.tag", HTTP_GET, handleGetTagInfo);

        server.on("/api/cogs.tags", HTTP_GET, listTags);
    }
}