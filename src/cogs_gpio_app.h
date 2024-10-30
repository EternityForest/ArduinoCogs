#pragma once
#include <map>
#include <string>
#include <vector>

#include "ArduinoJson.h"
#include "cogs_rules.h"

namespace cogs_gpio
{

    class GPIOInfo
    {
    public:
        int pin;
        char *name;
        bool in;
        bool out;
        int (*readFunction)(int) = nullptr;
        void (*writeFunction)(int, int) = nullptr;
    };
    extern std::vector<GPIOInfo *> gpioInfo;

    class CogsDigitalInput
    {
    public:
        void onNewRawDigitalValue(bool);

        bool activeHigh = false;
        bool lastInputLevel = false;
        unsigned long debounceTimestamp = 0;
        int debounce = 0;

        void setupDigitalTargetsFromJson(const JsonVariant &config);
        std::shared_ptr<cogs_rules::IntTagPoint> activeTarget = nullptr;
        std::shared_ptr<cogs_rules::IntTagPoint> inactiveTarget = nullptr;
        std::shared_ptr<cogs_rules::IntTagPoint> digitalValueTarget = nullptr;

        void configureDigitalPropsFromData(const JsonVariant &config);
    };

    class CogsSimpleInput : public CogsDigitalInput
    {
    public:
        int pin;
        int (*readFunction)(int) = nullptr;
        bool isInterruptDriven = false;

        explicit CogsSimpleInput(const JsonVariant &config);
        ~CogsSimpleInput();
        virtual void poll();
    };

    extern std::vector<CogsSimpleInput> simpleInputs;

    class CogsAnalogInput : public CogsDigitalInput
    {
    public:
        int pin;
        int (*readFunction)(int) = nullptr;

        /// Multiply the input by this value to get the real useful value
        float inputScaleFactor = 1.0;

        float filterTime = 0.0;

        float filteredValue = 0.0;

        /// The offset to subtract, in raw units.
        int centerValue = 0;
        float centerValueFloat = 0.0;

        /// The original, not auto drift corrected
        float absoluteZeroOffset = 0.0;

        int hystWindowCenter = 0;
        float hystWindowCenterFloat = 0.0;

        int hysteresis = 0;

        bool analogHysteresis = false;

        unsigned long lastDriftCorrection = 0;

        /// The t value for drift correction.
        float driftFactor = 0.0;

        /// The deadband around zero for the analog value
        float deadBand = 0.0;

        /// Do auto zeroing if the value is below this
        int autoZero = 0;

        // The value target, after processing
        std::shared_ptr<cogs_rules::IntTagPoint> analogValueTarget = nullptr;

        explicit CogsAnalogInput(const JsonVariant &config);
        virtual void poll();

        // Used in lock-in amplifier mode for reference
        void (*referenceWriteFunction)(int,int) = nullptr;

        int referencePin = -1;
    };

    class CogsSimpleOutput
    {
    public:
        unsigned int pin;
        int pwmSteps = 1;
        void (*writeFunction)(int, int) = nullptr;
        bool invert = false;
        explicit CogsSimpleOutput(const JsonVariant &config);
        ~CogsSimpleOutput();

        std::shared_ptr<cogs_rules::IntTagPoint> sourceTag = nullptr;
    };
    extern std::vector<CogsSimpleOutput> simpleOutputs;

    //! Declare pin is available to user config as an input.
    //! Name is arbitrary human readable title
    //! Pin is GPIO pin number, use a value above 1024 for a virtual pin
    //! Read function is optional, and only relevant with virtual pins
    void declareInput(const std::string &name, int pin, int (*)(int) = nullptr);

    //! Declare pin is available to user config as an output.
    //! Name is arbitrary human readable title
    //! Pin is GPIO pin number
    //! Pin is GPIO pin number, use a value above 1024 for a virtual pin
    //! Read function is optional, and only relevant with virtual pins
    void declareOutput(const std::string &name, int pin, void (*)(int, int) = nullptr);

    void begin();
}