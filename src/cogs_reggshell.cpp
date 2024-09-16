#include <Arduino.h>
#include "cogs_reggshell.h"
#include "cogs_global_events.h"

static void fastPoll(){
    if(Serial.available()){
        cogs_reggshell::interpreter->parseChar(Serial.read());
    }
}

void cogs_reggshell::initializeReggshell()
{
    if(cogs_reggshell::interpreter){
        return;
    }
    cogs_reggshell::interpreter = new reggshell::Reggshell();
    cogs::fastPollHandlers.push_back(fastPoll);
}