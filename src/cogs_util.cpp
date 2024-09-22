#include <Arduino.h>
#include <LittleFS.h>
#include "cogs_util.h"
#include "cogs_global_events.h"

static uint32_t randomState = 1;

namespace cogs{

 
     uint32_t random()  //flawfinder: ignore
    {
        /// Adding this seeds it with timing entropy.
        randomState += micros();
        // This is an XORshift function
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }

    uint32_t random(uint32_t min, uint32_t max)  //flawfinder: ignore
    {
        return random() % (max - min) + min;  //flawfinder: ignore
    }

    void ensureDirExists(const std::string &dir){
        if(!LittleFS.exists(dir.c_str())){
            LittleFS.mkdir(dir.c_str());
        }
    }

    void setDefaultFile(const std::string &fn, const std::string &content){
        std::string dirname = fn.substr(0, fn.rfind('/'));
        ensureDirExists(dirname);

        
        File f = LittleFS.open(fn.c_str(), "r");  //flawfinder: ignore
        if (f){
            if(f.size()> 0){
                f.close();
                return;
            }
            f.close();
        }

        File f2 = LittleFS.open(fn.c_str(), FILE_WRITE);  //flawfinder: ignore
        if(!f2){
            cogs::logError("Error opening file for writing: " + fn);
            return;
        }
        f2.write(reinterpret_cast<const uint8_t*>(content.c_str()), content.size());
        f2.close();
        cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, fn);
    };

    void logError(const std::string &msg){
        Serial.println(msg.c_str());
    }

    void logInfo(const std::string &msg){
        Serial.println(msg.c_str());
    }
};
