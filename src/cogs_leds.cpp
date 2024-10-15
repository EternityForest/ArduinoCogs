#include "cogs_leds.h"
#include "cogs_bindings_engine.h"
#include "cogs_global_events.h"

using namespace cogs_leds;

// We do this dirty checker thing once per frame because red, green, and blue
// get updated every separately and we want to push data all at once.
static void dirtyChecker(){
    if(dirty){
        dirty = false;
        xTaskNotifyGive(ledThreadHandle);
    }
}

static void ledsThread(void *arg)
{
    while (true)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        FastLED.show();
        xSemaphoreGive(mutex);
        
        // We refresh every 1000ms or so in case noise has messed up the state.
        ulTaskNotifyTake(pdTRUE, 10000);
    }
}

static void redTagHandler(cogs_rules::IntTagPoint *tp)
{
    dirty = true;
    // Since locking isn't critical, ensure we don't get completely jammed
    // if some unknown thing happens.
    xSemaphoreTake(mutex, 100);
    for (int i = 0; i < numLeds; i++)
    {
        leds[i].red = tp->value[i];
    }
    xSemaphoreGive(mutex);
}

static void greenTagHandler(cogs_rules::IntTagPoint *tp)
{
    dirty = true;
    xSemaphoreTake(mutex, 100);
    for (int i = 0; i < numLeds; i++)
    {
        leds[i].green = tp->value[i];
    }
    xSemaphoreGive(mutex);
}

static void blueTagHandler(cogs_rules::IntTagPoint *tp)
{
    dirty = true;
    xSemaphoreTake(mutex, 100);
    for (int i = 0; i < numLeds; i++)
    {
        leds[i].blue = tp->value[i];
    }
    xSemaphoreGive(mutex);
}

namespace cogs_leds
{
    bool dirty = false;
    CRGB *leds = 0;
    int numLeds = 0;
    SemaphoreHandle_t mutex = 0;
    TaskHandle_t ledThreadHandle = 0;

    void begin(CRGB * l, int numLeds)
    {

        cogs::registerFastPollHandler(dirtyChecker);

        mutex = xSemaphoreCreateMutex();
        xSemaphoreGive(mutex);

        if (!l)
        {
            cogs::logError("leds is null");
            throw std::runtime_error("leds is null");
        }

        leds = l;
        numLeds = numLeds;

        auto r = cogs_rules::IntTagPoint::getTag("leds.red", 0, numLeds);
        r->subscribe(&redTagHandler);
        auto g = cogs_rules::IntTagPoint::getTag("leds.green", 0, numLeds);
        g->subscribe(&greenTagHandler);
        auto b = cogs_rules::IntTagPoint::getTag("leds.blue", 0, numLeds);
        b->subscribe(&blueTagHandler);

        xTaskCreate(ledsThread, "ledsThread", 8192, 0, 19, &ledThreadHandle);
    }
}