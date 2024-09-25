#pragma once
/**
 * @file
 * This has static web content. Note that cogs never uses server side templating.
 */

#include "web/data/cogs_json_schemas.h"
#include "web/generated_data/file_manager_app_js_gz.h"
#include "web/generated_data/lit_min_js_gz.h"
#include "web/generated_data/jsoneditor_min_js_gz.h"
#include "web/generated_data/barrel_css_gz.h"
#include "web/generated_data/cogs_js_gz.h"
#include "web/generated_data/page_template_html_gz.h"
#include "web/generated_data/json_editor_app_js_gz.h"

void setup_builtin_static()
{
    cogs_web::server.on("/builtin/lit.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, lit_min_js_gz, sizeof(lit_min_js_gz), "application/javascript"); });

    cogs_web::server.on("/builtin/jsoneditor.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, jsoneditor_min_js_gz, sizeof(jsoneditor_min_js_gz), "application/javascript"); });

    cogs_web::server.on("/builtin/barrel.css", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, barrel_css_gz, sizeof(barrel_css_gz), "text/css"); });

    cogs_web::server.on("/builtin/jsoneditor_app.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, json_editor_app_js_gz, sizeof(json_editor_app_js_gz), "application/javascript"); });

    cogs_web::server.on("/builtin/files_app.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request,
                                                 file_manager_app_js_gz,
                                                 sizeof(file_manager_app_js_gz),
                                                 "application/javascript"); });

    cogs_web::server.on("/builtin/schemas/object.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", generic_object_schema); });

    cogs_web::server.on("/builtin/schemas/network.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", wifi_schema); });

    cogs_web::server.on("/builtin/schemas/device.json", HTTP_GET, [](AsyncWebServerRequest *request)
                        { request->send(200, "application/json", device_schema); });

    // Automation schema is included on demand in editable automation
    cogs_web::server.on("/builtin/cogs.js", HTTP_GET, [](AsyncWebServerRequest *request)
                        { cogs_web::sendGzipFile(request, cogs_js_gz, sizeof(cogs_js_gz), "application/javascript"); });
}