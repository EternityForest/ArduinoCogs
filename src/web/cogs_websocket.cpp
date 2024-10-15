#include <ArduinoJson.h>
#include "cogs_web.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "cogs_bindings_engine.h"

AsyncWebSocket ws("/api/ws");
static void pushTagPointValue(cogs_rules::IntTagPoint *tp);

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
                if (t->value[0] != v)
                {
                    pushTagPointValue(t.get());
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

static void pushTagPointValue(cogs_rules::IntTagPoint *tp)
{
    JsonDocument doc;
    doc["vars"][tp->name] = tp->value[0];
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

void cogs_web::exposeTagPoint(std::shared_ptr<cogs_rules::IntTagPoint> tp)
{
    tp->subscribe(pushTagPointValue);
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
        cogs::logError("WS Err");
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
    ws.cleanupClients(8);
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