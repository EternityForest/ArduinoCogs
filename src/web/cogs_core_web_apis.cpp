
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

static std::string uploadFileName = "";
static inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}


static void navbar_handler(AsyncWebServerRequest *request)
{
    cogs::lock();
    JsonDocument doc;

    int ctr = 0;
    for (auto const &e : navBarEntries)
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
        cogs::unlock();
        return;
    }

    auto dir = LittleFS.open(request->arg("dir").c_str(), "r");
    if (!dir)
    {
        request->send(404);
        cogs::unlock();
        return;
    }
    if (!dir.isDirectory())
    {
        request->send(404);
        cogs::unlock();
        return;
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

    cogs::unlock();
}

static void handleDownload(AsyncWebServerRequest *request)
{
    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        cogs::unlock();
        return;
    }

    request->send(LittleFS, request->arg("file").c_str());

    cogs::unlock();
}

static void handleUpload(AsyncWebServerRequest *request, String orig_filename, size_t index, uint8_t *data, size_t len, bool final)
{
    cogs::lock();
    uploadFileName = orig_filename.c_str();

    if (!index)
    {
        // open the file on first call and store the file handle in the request object
        request->_tempFile = LittleFS.open("/upload_temp.bin", "w");

        if (!request->_tempFile)
        {
            cogs::logError("Can't open tempfile");
            request->send(500, "text/plain", "cantopenfile");
            cogs::unlock();
            return;
        }
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
    }

    cogs::unlock();
}

static void handleSetFile(AsyncWebServerRequest *request)
{
    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        cogs::unlock();
        return;
    }

    if (!request->hasArg("data"))
    {
        request->send(500, "text/plain", "nodata");
        cogs::unlock();
        return;
    }
    auto f = LittleFS.open(request->arg("file").c_str(), "w");
    if (!f)
    {
        request->send(500, "text/plain", "cantopenfile");
        cogs::unlock();
        return;
    }

    f.print(request->arg("data").c_str());
    f.close();
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);

    cogs::unlock();
}

static void handleDeleteFile(AsyncWebServerRequest *request)
{

    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        return;
    }

    std::string path = request->arg("file").c_str();
    if (path.size() == 0)
    {
        request->send(500, "text/plain", "emptypath");
        cogs::unlock();
        return;
    }
    if (ends_with(path, "/"))
    {
        path = path.substr(0, path.size() - 1);
    }
    LittleFS.remove(request->arg("file").c_str());
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    request->send(200);

    cogs::unlock();
}

static void handleRenameFile(AsyncWebServerRequest *request)
{

    cogs::lock();
    if (!request->hasArg("file"))
    {
        request->send(500, "text/plain", "nofileparam");
        cogs::unlock();
        return;
    }
    if (!request->hasArg("newname"))
    {
        request->send(500, "text/plain", "nofileparam");
        cogs::unlock();
        return;
    }

    std::string fn = request->arg("newname").c_str();
    std::string dirname = fn.substr(0, fn.rfind('/'));
    cogs::ensureDirExists(dirname);

    LittleFS.rename(request->arg("file").c_str(), request->arg("newname").c_str());
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("file").c_str());
    cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, request->arg("newname").c_str());

    request->send(200);

    cogs::unlock();
}

static void handleGetTagInfo(AsyncWebServerRequest *request)
{

    cogs::lock();
    if (!request->hasArg("tag"))
    {
        request->send(500, "text/plain", "nofileparam");
        cogs::unlock();
        return;
    }
    std::string tagname = request->arg("tag").c_str();
    if (!cogs_rules::IntTagPoint::exists(tagname))
    {
        request->send(500, "text/plain", "tagnotfound");
        cogs::unlock();
        return;
    }
    auto tag = cogs_rules::IntTagPoint::getTag(tagname, 0, 1);

    if (!tag)
    {
        request->send(500, "text/plain", "tagnotfound");
        cogs::unlock();
        return;
    }

    JsonDocument doc;
    doc["max"] = tag->max;
    doc["min"] = tag->min;
    doc["unit"] = std::string(tag->unit->c_str());
    doc["step"] = 1.0 / float(tag->scale);
    doc["hi"] = tag->hi;
    doc["lo"] = tag->lo;
    doc["scale"] = tag->scale;
    doc["firstValue"] = tag->value[0];
    doc["length"] = tag->count;

    char buf[256];
    serializeJson(doc, buf, 256);

    request->send(200, "text/plain", buf);

    cogs::unlock();
}

static void listTags(AsyncWebServerRequest *request)
{
    cogs::lock();
    JsonDocument doc = JsonDocument();
    for (auto const &tagPoint : cogs_rules::IntTagPoint::all_tags)
    {
        doc["tags"][tagPoint->name] = tagPoint->value[0];
    }

    char buf[2048];
    serializeJson(doc, buf, 2048);

    request->send(200, "text/plain", buf);

    cogs::unlock();
}

static void uploadFinal(AsyncWebServerRequest *request)
{
    cogs::lock();

    std::string path;

    if (request->hasArg("path"))
    {
        path = request->arg("path").c_str();
        if (path.size() == 0)
        {
            path = "/";
        }

        // Allow auto using the filename
        if (!ends_with(path, "/"))
        {
            path = path + "/";
        };

        path = path + uploadFileName.c_str();

        if (!LittleFS.rename("/upload_temp.bin", path.c_str()))
        {
            request->send(500, "text/plain", "renamefailed");
            cogs::unlock();
            return;
        }
        cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, path);
    }
    else
    {
        request->send(500, "text/plain", "nofnparam");
        cogs::unlock();
        return;
    }

    std::string redirect = request->url().c_str();
    if (request->hasArg("redirect"))
    {
        redirect = request->arg("redirect").c_str();
        request->redirect(redirect.c_str());
    }   
    else{
        request->send(200, "text/plain", "ok");
    }

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

        server.on("/api/cogs.upload", HTTP_POST, uploadFinal, handleUpload);

        server.on("/api/cogs.setfile", HTTP_POST, handleSetFile);

        server.on("/api/cogs.download", HTTP_GET, handleDownload);

        server.on("/api/cogs.deletefile", HTTP_POST, handleDeleteFile);
        server.on("/api/cogs.renamefile", HTTP_POST, handleRenameFile);

        server.on("/api/cogs.tag", HTTP_GET, handleGetTagInfo);

        server.on("/api/cogs.tags", HTTP_GET, listTags);
    }
}