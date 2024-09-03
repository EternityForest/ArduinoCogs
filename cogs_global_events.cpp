
#include "cogs_global_events.h"

using namespace cogs;

std::list<void (*)(GlobalEvent, int, std::string)> global_events;

void triggerGlobalEvent(GlobalEvent event, int param = 0, std::string param2 = ""){
    for (auto e : global_events)
    {
        e(event, param, param2);
    }
}