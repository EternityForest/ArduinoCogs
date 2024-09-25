#pragma once
/**
 * @file
 * This has static web content. Note that cogs never uses server side templating.
 */

#include "web/data/jsoneditor.h"
#include "web/data/lit_js.h"
#include "web/data/cogs_json_edit_page.h"
#include "web/data/cogs_json_schemas.h"
#include "web/data/cogs_js_lib.h"
#include "web/data/cogs_barrel_css.h"
#include "web/data/cogs_file_manager_page.h"

void sendCacheableFile(AsyncWebServerRequest *request,
                       const char *data,
                       unsigned int size,
                       const char *mime)
{
    AsyncWebServerResponse *response = request->beginResponse(200,
                                                              mime,
                                                              (const uint8_t *)data,
                                                              size);
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
}

void setup_builtin_static()
{
    cogs_web::server.on("/builtin/lit.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        {
                    AsyncWebServerResponse *response = request->beginResponse(200, 
                    "application/javascript",
                    lit_min_js, 
                    sizeof(lit_min_js));

                    response->addHeader("Content-Encoding", "gzip");
                    request->send(response); });

    cogs_web::server.on("/builtin/jsoneditor.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        {
                    AsyncWebServerResponse *response = request->beginResponse(200, 
                    "application/javascript",
                    jsoneditor_min_js_gz, 
                    sizeof(jsoneditor_min_js_gz));

                    response->addHeader("Content-Encoding", "gzip");
                    request->send(response); });

    cogs_web::server.on("/builtin/barrel.css", HTTP_GET, [](AsyncWebServerRequest *request)
                        {
                    AsyncWebServerResponse *response = request->beginResponse(200, 
                    "text/css",
                    barrel_min_css, 
                    sizeof(barrel_min_css));
                    response->addHeader("Content-Encoding", "gzip");
                    request->send(response); });

    cogs_web::server.on("/builtin/jsoneditor_app.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/javascript", jsoneditor_js); });

    cogs_web::server.on("/builtin/files_app.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { sendCacheableFile(request,
                                            filemanager_js,
                                            sizeof(filemanager_js),
                                            "application/javascript"); });

    cogs_web::server.on("/builtin/schemas/object.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", generic_object_schema); });

    cogs_web::server.on("/builtin/schemas/network.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", wifi_schema); });

    cogs_web::server.on("/builtin/schemas/device.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", device_schema); });

    // Automation schema is included on demand in editable automation
    cogs_web::server.on("/builtin/cogs.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/javascript", cogs_js_lib); });
}