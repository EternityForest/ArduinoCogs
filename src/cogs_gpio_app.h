#pragma once
#include <map>
#include <string>
#include <vector>

#include "ArduinoJson.h"
#include "cogs_bindings_engine.h"

namespace cogs_gpio{

    //! Map names(Which are arbitrary human readable titles) to GPIO pin numbers.
    extern std::map<std::string, int> availableInputs;
    
    //! Map names(Which are arbitrary human readable titles) to GPIO pin numbers.
    extern std::map<std::string, int> availableOutputs;


    //! Map "virtual" pin numbers to functions that read input values.
    extern std::map<int, bool (*)()> inputFunctions;
    //! Map "virtual" pin numbers to functions that write output values.
    extern std::map<int, void (*)(int)> outputFunctions;


    class CogsSimpleInput
    {
    public:
        int pin;
        bool (*readFunction)() = nullptr;
        bool isInterruptDriven = false;
        bool activeHigh = false;
        bool lastInputLevel = false;
        explicit CogsSimpleInput(const JsonVariant &config);
        ~CogsSimpleInput();
        virtual void poll();

        void onNewRawDigitalValue(bool);

        void setupTargetsFromJson(const JsonVariant &config);

        std::shared_ptr<cogs_rules::IntTagPoint> activeTarget = nullptr;
        std::shared_ptr<cogs_rules::IntTagPoint> inactiveTarget = nullptr;
    };

    extern std::vector<CogsSimpleInput> simpleInputs;


    class CogsSimpleOutput
    {
    public:
        int pin;
        int pwmSteps = 1;
        void (*writeFunction)(int) = nullptr;
        bool invert = false;
        explicit CogsSimpleOutput(const JsonVariant &config);
        ~CogsSimpleOutput();

        std::shared_ptr<cogs_rules::IntTagPoint> sourceTag = nullptr;
    };
    extern std::vector<CogsSimpleOutput> simpleOutputs;




    //! Declare pin is available to user config as an input.
    //! Name is arbitrary human readable title
    //! Pin is GPIO pin number
    void declareInput(const std::string &name, int pin);

    //! Declare pin is available to user config as an output.
    //! Name is arbitrary human readable title
    //! Pin is GPIO pin number
    void declareOutput(const std::string &name, int pin);

    void begin();
}