#include <Arduino.h>
#include "cogs_reggshell.h"

namespace cogs_i2c{

    bool i2cStarted = false;

    static void scanCommand(reggshell::Reggshell * interpreter, const char * arg1, const char * arg2, const char * arg3)
    {
        interpreter->println("Scanning I2C");
        for (int i = 0; i < 127; i++)
        {
            Wire.beginTransmission(i);
            if (Wire.endTransmission() == 0)
            {
                interpreter->println(i);
            }
        }

        interpreter->println("Done");
    }
    void begin(int sda, int scl){
        if(i2cStarted){
            return;
        }
        Wire.setPins(sda, scl);
        Wire.begin();
        i2cStarted = true;

        reggshell::Reggshell::addCommand("scan", scanCommand);
    }

}