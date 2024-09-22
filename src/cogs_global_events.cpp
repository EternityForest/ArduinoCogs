
#include "cogs_global_events.h"

using namespace cogs;

namespace cogs
{
    unsigned long lastSlowPoll = 0;
    unsigned int slowPollIndex = 0;


    std::list<void (*)()> fastPollHandlers;
    std::vector<void (*)()> slowPollHandlers;

    std::list<void (*)(cogs::GlobalEvent, int,const std::string &)> globalEventHandlers;


    // Slow poll happens one at a time, and only one handler at a time,
    // so they don't all happen at once and make lag.
    static void doSlowPoll(){
        if(slowPollIndex >= slowPollHandlers.size()){
            return;
        }

        slowPollIndex++;
        slowPollHandlers[slowPollIndex % slowPollHandlers.size()]();
    }


    void poll(){
        for (auto e : fastPollHandlers){
            e();
        }

        if (millis() - lastSlowPoll > 1000){
            lastSlowPoll = millis();
            doSlowPoll();
        } 
    }
    void triggerGlobalEvent(GlobalEvent event, int param1, const std::string &param2)
    {
        for (auto e : globalEventHandlers)
        {
            e(event, param1, param2);
        }
    }

}