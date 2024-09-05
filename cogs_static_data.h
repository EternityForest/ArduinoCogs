#pragma once
/**
 * @file
 * This has static web content. Note that cogs never uses server side templating.
 */

#include "webdata/jsoneditor.h"
#include "webdata/lit_js.h"
#include "webdata/cogs_json_edit_page.h"
#include "webdata/cogs_json_schemas.h"

void setup_builtin_static(){
    cogs_web::server.on("/builtin/lit.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                    AsyncWebServerResponse *response = request->beginResponse(200, 
                    "application/javascript",
                    lit_min_js, 
                    sizeof(lit_min_js));

                    response->addHeader("Content-Encoding", "gzip");
                    request->send(response); 
            });

    cogs_web::server.on("/builtin/jsoneditor.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                    AsyncWebServerResponse *response = request->beginResponse(200, 
                    "application/javascript",
                    jsoneditor_min_js_gz, 
                    sizeof(jsoneditor_min_js_gz));

                    response->addHeader("Content-Encoding", "gzip");
                    request->send(response); 
            });


        cogs_web::server.on("/builtin/jsoneditor.html", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/html", jsoneditor_html); });

        cogs_web::server.on("/builtin/schemas/object.json", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200,"application/json", generic_object_schema);
        });


}