#pragma once
#include <string>
#include "cogs_util.h"
#include <ArduinoJson.h>
#include <Arduino.h>

namespace cogs_rules
{

    // Goes in between expressions and targets
    class Filter
    {

    public:
        // Num is the number of data channels
        // Resolution is fixed point resolution
        Filter(int num, int resolution, const JsonObject &json);
        Filter();

        // In-place filtering. Return value can be false to completely drop a sample
        virtual bool sample(int *input, int * current_target_vals);

        // Used for things like lowpass filters, when a binding
        // Becomes active we need to know the previous state
        virtual void setStateEnterTargetValue(int *input);
        virtual ~Filter();
    };

    extern Filter *createFilter(int num, int resolution, const JsonObject &json);
}