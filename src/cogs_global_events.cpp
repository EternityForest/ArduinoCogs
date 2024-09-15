
#include "cogs_global_events.h"

using namespace cogs;

namespace cogs
{

    std::list<void (*)(cogs::GlobalEvent, int, std::string)> globalEventHandlers;

    void triggerGlobalEvent(GlobalEvent event, int param1, std::string param2)
    {
        for (auto e : globalEventHandlers)
        {
            e(event, param1, param2);
        }
    }

}