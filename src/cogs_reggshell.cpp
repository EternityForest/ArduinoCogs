#include <Arduino.h>
#include "cogs_reggshell.h"
#include "cogs_global_events.h"
#include <atomic>

#if defined(ESP32) || defined(ESP8266)
#include "esp_log.h"
#endif

reggshell::Reggshell * cogs_reggshell::interpreter = NULL;

static std::atomic<bool> enableSerial(true);

static void fastPoll(){
    while (Serial.available()){
        if(enableSerial.load()){
            cogs_reggshell::interpreter->parseChar(Serial.read()); //flawfinder: ignore
        }
    }
}

void cogs_reggshell::exec(std::string & cmd){
    enableSerial = false;    
    cogs_reggshell::interpreter->clearLineBuffer();

    // On char at a time
    for(int i = 0; i < cmd.length(); i++){
        cogs_reggshell::interpreter->parseChar(cmd[i]); //flawfinder: ignore
    }
    cogs_reggshell::interpreter->parseChar('\n');
    cogs_reggshell::interpreter->clearLineBuffer();
    while (Serial.available())
    {
        Serial.read();
    }
    enableSerial = true;
}
void cogs_reggshell::begin()
{
    if(cogs_reggshell::interpreter){
        return;
    }

    #if defined(ESP32) || defined(ESP8266)
    esp_log_level_set("*",ESP_LOG_WARN);
    #endif
    cogs_reggshell::interpreter = new reggshell::Reggshell();
    cogs::fastPollHandlers.push_back(fastPoll);

    Serial.println("Reggshell initialized");
}