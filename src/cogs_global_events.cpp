
#include "cogs_global_events.h"
#include "cogs_util.h"

using namespace cogs;

namespace cogs
{
    unsigned long lastSlowPoll = 0;
    unsigned int slowPollIndex = 0;

    volatile int lastFastPollTime = 0;

    std::vector<void (*)()> fastPollHandlers;
    std::vector<void (*)()> slowPollHandlers;

    std::vector<void (*)(cogs::GlobalEvent, int, const std::string &)> globalEventHandlers;

    // Slow poll happens one at a time, and only one handler at a time,
    // so they don't all happen at once and make lag.
    // but if they all take less that 2ms we'll do them all.
    static void doSlowPoll()
    {
        int m = slowPollHandlers.size();
        unsigned long start = millis();

        while ((m>0) && (millis() - start < 2)){
            m--;
            slowPollIndex++;
            slowPollHandlers[slowPollIndex % slowPollHandlers.size()]();
        }
    }

    void poll()
    {
        unsigned long now = micros();
        for (auto const &e : fastPollHandlers)
        {
            e();
        }
        lastFastPollTime = micros()-now;

        if (millis() - lastSlowPoll > 1000)
        {
            if(lastFastPollTime>50000){
                Serial.println("slow poll cycle ");
                Serial.println(lastFastPollTime);
            }
            lastSlowPoll = millis();
            doSlowPoll();
        }
    }
    void triggerGlobalEvent(GlobalEvent event, int param1, const std::string &param2)
    {
        for (auto const &e : globalEventHandlers)
        {
            try
            {
                e(event, param1, param2);
            }
            catch (std::exception err)
            {
                cogs::logError("Error in global event handler for " +
                               std::to_string(event) + ":\n" +
                               std::string(err.what()));
            }
        }
    }

    void registerFastPollHandler(void (*handler)())
    {
        for (const auto i : fastPollHandlers)
        {
            if (i == handler)
            {
                return;
            }
        }
        fastPollHandlers.push_back(handler);
    }
    void unregisterFastPollHandler(void (*handler)())
    {

        //std erase doesn't work yet on platformio for some reason
        fastPollHandlers.erase(std::remove(fastPollHandlers.begin(), fastPollHandlers.end(), handler), fastPollHandlers.end());
    }

}