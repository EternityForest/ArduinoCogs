#include <Arduino.h>
#include <LittleFS.h>
#include "cogs_util.h"

namespace cogs{
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
