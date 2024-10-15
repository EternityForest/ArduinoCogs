#pragma once
#include "reggshell/reggshell.h"

namespace cogs_reggshell
{
    extern reggshell::Reggshell *interpreter;
    void exec(std::string & command);

    //!  Enable the serial terminal shell on
    //!  the serial port.
    void begin();
}