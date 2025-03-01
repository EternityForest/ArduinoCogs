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
        bool danger=false;
        bool in=false;
        bool out=false;
        int (*readFunction)(int) = nullptr;
        void (*writeFunction)(int, int) = nullptr;
    };
    extern std::vector<GPIOInfo *> gpioInfo;

    class CogsDigitalInput
    {
    public:
        void onNewRawDigitalValue(bool);
        int pin;
        bool activeHigh = false;
        bool lastInputLevel = false;
        unsigned long debounceTimestamp = 0;
        int debounce = 0;
        bool pullup=false;


        void setupDigitalTargetsFromJson(const JsonVariant &config);
        std::shared_ptr<cogs_rules::IntTagPoint> activeTarget = nullptr;
        std::shared_ptr<cogs_rules::IntTagPoint> inactiveTarget = nullptr;
        std::shared_ptr<cogs_rules::IntTagPoint> digitalValueTarget = nullptr;

        void configureDigitalPropsFromData(const JsonVariant &config);

        // If this is true, we expect there to always be a 1M or similar resistor
        // Connected between the pin and ground, allowing us to be instantly notified about
        // a bad connection
        bool needPilotResistor = false;
        bool testResistorPresence();

        bool pilotResistorPresent = true;
    };

    class CogsSimpleInput : public CogsDigitalInput
    {
    public:
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
        int channel;
        int (*readFunction)(int) = nullptr;

        int rawToScaledMultiplier = 1.0;

        // what to mult adc untis by to get the tag point fixed point vals
        int valMult=0;

        /// The offset to subtract, in raw units.
        int rawUnitCenterValue = 0;



        bool singleEndedSense = false;

        float rawUnitMaxAutoZero = 0;



        // The value target, after processing
        std::shared_ptr<cogs_rules::IntTagPoint> analogValueTarget = nullptr;

        explicit CogsAnalogInput(const JsonVariant &config);
        virtual void poll();

        inline int rawRead();

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

    //! Mark a pin as dangerous
    void markDangerous(const std::string &name);

    void begin();
}