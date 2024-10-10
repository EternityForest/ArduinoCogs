#include <Arduino.h>
#include "cogs_reggshell.h"
#include "cogs_global_events.h"

reggshell::Reggshell * cogs_reggshell::interpreter = NULL;

static void fastPoll(){
    if(Serial.available()){
        cogs_reggshell::interpreter->parseChar(Serial.read()); //flawfinder: ignore
    }
}

void cogs_reggshell::begin()
{
    if(cogs_reggshell::interpreter){
        return;
    }
    cogs_reggshell::interpreter = new reggshell::Reggshell();
    cogs::fastPollHandlers.push_back(fastPoll);

    Serial.println("Reggshell initialized");
}