#include <ArduinoJson.h>
#include "cogs_web.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "cogs_rules.h"

AsyncWebSocket ws("/api/ws");
static void pushTagPointValue(cogs_rules::IntTagPoint *tp, bool force = false);

static void handleWsData(char *d)
{
    JsonDocument doc;
    deserializeJson(doc, d);

    // Vars is going to be a dict of things to set the values of
    if (doc["vars"].is<JsonObject>())
    {
        cogs::lock();
        for (auto kv : doc["vars"].as<JsonObject>())
        {
            int v = kv.value().as<int>();
            if (cogs_rules::IntTagPoint::exists(kv.key().c_str()))
            {
                auto t = cogs_rules::IntTagPoint::getTag(kv.key().c_str(), 0, 1);
                t->setValue(v, 0, 1);

                // If for some reason the set fails to change the value
                // inform the clients of that
                if (t->value[0] != v)
                {
                    pushTagPointValue(t.get(), true);
                }
            }
        }
        cogs::unlock();
    }
}

void cogs_web::wsBroadcast(const char *key, const JsonVariant &val)
{
    JsonDocument doc;
    doc["vars"][key] = val;
    char *buf = reinterpret_cast<char *>(malloc(512));
    if (!buf)
    {
        cogs::logError("malloc failed");
        return;
    }
    serializeJson(doc, buf, 512);
    ws.textAll(buf);
    free(buf);
}

void cogs_web::wsBroadcast(const char *key, const char *data)
{
    JsonDocument doc;
    doc["vars"][key] = data;
    char *buf = reinterpret_cast<char *>(malloc(512));
    if (!buf)
    {
        cogs::logError("malloc failed");
        return;
    }
    serializeJson(doc, buf, 512);
    ws.textAll(buf);
    free(buf);
}


// Iterating all tags is slow, we don't want to do that too often
// so we only do it if there is a change
static bool atLeastOneTagDirty = false;

static void pushTagPointValue(cogs_rules::IntTagPoint *tp, bool force)
{
    if(!force){
        if(!tp->webAPIDirty){
            return;
        }
        // Frequently changing tags can wait until the next poll.
        if((millis() - tp->lastChangeTime) < 250){
            atLeastOneTagDirty = true;
            return;    
        }
    }

    JsonDocument doc;
    doc["vars"][tp->name] = tp->value[0];
    char *buf = reinterpret_cast<char *>(malloc(256));
    if (!buf)
    {
        cogs::logError("malloc failed");
        return;
    }
    serializeJson(doc, buf, 256);
    ws.textAll(buf);
    free(buf);
    tp->webAPIDirty = false;
}

static void onChange(cogs_rules::IntTagPoint *tp){
    pushTagPointValue(tp, false);
}
static void pushAllDirtyTags(){
    if(!atLeastOneTagDirty){
        return;
    }
    for(auto const &tagPoint : cogs_rules::IntTagPoint::all_tags){
        if(tagPoint->webAPIDirty){
            pushTagPointValue(tagPoint.get(), true);
        }
    }
    atLeastOneTagDirty = false;
}

void cogs_web::exposeTagPoint(std::shared_ptr<cogs_rules::IntTagPoint> tp)
{
    tp->subscribe(onChange);
}

static void tagCreatedHandler(cogs::GlobalEvent evt, int dummy, const std::string &name)
{
    if (evt != cogs::tagCreatedEvent)
    {
        return;
    }

    auto t = cogs_rules::IntTagPoint::getTag(name, 0, 1);
    if (name[0] == '_')
    {
        return;
    }
    else
    {
        cogs_web::exposeTagPoint(t);
    }
}

static void onEvent(AsyncWebSocket *server,
                    AsyncWebSocketClient *client,
                    AwsEventType type,
                    void *arg, uint8_t *data, size_t len)
{

    if (type == WS_EVT_CONNECT)
    {
        client->ping();
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        //
    }
    else if (type == WS_EVT_ERROR)
    {
        //cogs::logError("WS Err");
    }
    else if (type == WS_EVT_PONG)
    {
        //
    }
    else if (type == WS_EVT_DATA)
    {
        // data packet
        AwsFrameInfo *info = reinterpret_cast<AwsFrameInfo *>(arg);
        if (info->final && info->index == 0 && info->len == len)
        {
            // the whole message is in a single frame and we got all of it's data
            if (info->opcode == WS_TEXT)
            {
                data[len] = 0;
                try
                {
                    handleWsData((char *)data);
                }
                catch (std::exception &e)
                {
                    cogs::logError(e.what());   
                }
            }
        }
        else
        {
            cogs::logError("Multiple frames not supported");
        }
    }
}

void slowPoll()
{
    ws.cleanupClients(12);
    pushAllDirtyTags();
}

void cogs_web::setupWebSocketServer()
{
    ws.onEvent(onEvent);
    cogs_web::server.addHandler(&ws);
    cogs::slowPollHandlers.push_back(slowPoll);

    cogs::globalEventHandlers.push_back(tagCreatedHandler);

    for (auto const &tp : cogs_rules::IntTagPoint::all_tags)
    {
        if (tp->name[0] == '_')
        {
            continue;
        }
        cogs_web::exposeTagPoint(tp);
    }
}