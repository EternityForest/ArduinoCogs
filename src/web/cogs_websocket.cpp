#include <ArduinoJson.h>
#include "cogs_web.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "cogs_bindings_engine.h"

AsyncWebSocket ws("/api/ws");


static void handleWsData(char *){

}

void cogs_web::wsBroadcast(const char * key, const char * data){
    JsonDocument doc;
    doc[key]= data;
    char * buf = reinterpret_cast<char *>(malloc(512));
    if(!buf){
       cogs::logError("malloc failed");
       return;
    }
    serializeJson(doc, buf, 512);
    ws.textAll(buf);
    free(buf);  
}

static void pushTagPointValue(cogs_rules::IntTagPoint *tp){
    JsonDocument doc;
    doc["vals"][tp->name] = tp->value[0];
    char * buf = reinterpret_cast<char *>(malloc(512));
    if(!buf){
       cogs::logError("malloc failed");
       return;
    }
    serializeJson(doc, buf, 512);
    ws.textAll(buf);
    free(buf);
}

void cogs_web::exposeTagPoint(std::shared_ptr<cogs_rules::IntTagPoint> tp){
    tp->subscribe(pushTagPointValue);
}

static void onEvent(AsyncWebSocket *server,
             AsyncWebSocketClient *client,
             AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{

    if (type == WS_EVT_CONNECT)
    {
        JsonDocument doc;

        for(auto tp: cogs_rules::IntTagPoint::all_tags){
            doc['vals'][tp.first] = tp.second->value[0];
        }
        char * buf = reinterpret_cast<char *>(malloc(8192));
        if(!buf){
           cogs::logError("malloc failed");
           return;
        }
        serializeJson(doc, buf, 8192);
        client->text(buf);
        free(buf);
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        //
    }
    else if (type == WS_EVT_ERROR)
    {
        cogs::logError("WS Err");
    }
    else if (type == WS_EVT_PONG)
    {
     //
    }
    else if (type == WS_EVT_DATA)
    {
        // data packet
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len)
        {
            // the whole message is in a single frame and we got all of it's data
            if (info->opcode == WS_TEXT)
            {
                data[len] = 0;
                handleWsData((char *)data);
            }
        
        }
        else
        {
            cogs::logError("Multiple frames not supported");
        }
    }
}

void cogs_web::setupWebSocketServer(){
    ws.onEvent(onEvent);
}