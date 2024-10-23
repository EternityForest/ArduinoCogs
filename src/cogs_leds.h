#include <Arduino.h>
#include <FastLED.h>
#include <freertos/semphr.h>

namespace cogs_leds{
    extern bool dirty;
    extern CRGB *leds;
    extern int numLeds;
    extern SemaphoreHandle_t mutex;
    extern TaskHandle_t ledThreadHandle;
    void begin(CRGB *, int);
    void refresh();
}