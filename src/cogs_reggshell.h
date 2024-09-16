#pragma once
#include "reggshell/reggshell.h"

namespace cogs_reggshell
{
    reggshell::Reggshell *interpreter;

    //!  Enable the serial terminal shell on
    //!  the serial port.
    void initializeReggshell();
}