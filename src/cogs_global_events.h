#pragma once
#include <string>
#include <list>
#include <vector>
#include <Arduino.h>

namespace cogs
{
    /// All global event types are defined here
    enum GlobalEvent
    {
        /// Called right after boot
        bootEvent = 0,

        /// Called before shutdown or deep sleep
        shutdownEvent = 1,

        /// Called right before light sleep
        beforeSleepEvent = 2,

        /// Called right after wake from light sleep
        afterWakeEvent = 3,

        /// Called when a file has changed, including additions or being deleted
        /// Not all files will have this event, it must be manually called from whatever
        /// Changes it
        /// Param0: ignored, param1: file name
        fileChangeEvent = 4,


        /// A tag point has been created
        tagCreatedEvent = 5


    };

    /// A GlobalEvent has an int and string parameter, either or both may be unused
    /// depending on the event type.
    /*!

    @example
    void someFunction(GlobalEvent event, int param, const std::string & param2){
     // do stuff
    }

    cogs::globalEventHandlers.push_back(someFunction);

    */
    extern std::list<void (*)(cogs::GlobalEvent, int, const std::string &)> globalEventHandlers;


    //! Put functions here if you want them to be called every loop
    extern std::list<void (*)()> fastPollHandlers;


    //! Put functions here if you want them to be called every few seconds.
    extern std::vector<void (*)()> slowPollHandlers;

    //! Trigger a global event.  All handlers will be called.
    //! @example cogs::triggerGlobalEvent(cogs::bootEvent);

    void triggerGlobalEvent(GlobalEvent event, int param1, const std::string &param2 );

    //! Poll all registered loop functions.
    //! Call this in a loop as fast as you can!
    void poll();
}