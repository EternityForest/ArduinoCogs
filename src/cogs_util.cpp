#include <Arduino.h>
#include <LittleFS.h>
#include "cogs_util.h"

static uint32_t randomState = 1;

namespace cogs{

 
     uint32_t random()
    {
        /// Adding this seeds it with timing entropy.
        randomState += micros();
        // This is an XORshift function
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }

    uint32_t random(uint32_t min, uint32_t max)
    {
        return random() % (max - min) + min;
    }

    void setDefaultFile(std::string fn, std::string content){
        File f = LittleFS.open(fn.c_str(), "r");
        if (f){
            if(f.size()> 0){
                f.close();
                return;
            }
            f.close();
        }

        f = LittleFS.open(fn.c_str(), "w");
        if(!f){
            cogs::logError("Error opening default file");
            return;
        }
        f.write((const uint8_t*)content.c_str(), content.size());
        f.close();
    };

    void logError(std::string msg){
        Serial.println(msg.c_str());
    }
};
