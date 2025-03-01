#include "cogs_gpio_app.h"
#include "cogs_global_events.h"
#include "cogs_rules.h"
#include "web/cogs_web.h"
#include "cogs_power_management.h"
#include "cogs_trouble_codes.h"
#include "editable_automation.h"

#include "web/generated_data/gpio_schema_json_gz.h"
#if defined(ESP32) || defined(ESP8266)
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#endif
#include <LittleFS.h>
#include <Arduino.h>

#define ANREAD analogRead

namespace cogs_gpio
{
    std::vector<GPIOInfo *> gpioInfo;

    std::vector<CogsSimpleInput> simpleInputs;
    std::vector<CogsSimpleOutput> simpleOutputs;
    std::vector<CogsAnalogInput> analogInputs;

    bool pullupEnabledOnPin(int pin)
    {
        // iterate all analog
        for (auto i : analogInputs)
        {
            if (i.pin == pin)
            {
                if (i.pullup)
                {
                    return true;
                }
            }
        }
        for (auto i : simpleInputs)
        {
            if (i.pin == pin)
            {
                if (i.pullup)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool foundError = false;

    // Always return higher values for touch.
    // Values may be positive or negative, they are inverted on some platforms
    int swCapsenseRead(int pin)
    {
        int val = 0;

        // #if defined(ESP32) || defined(ESP8266)
        //         if (digitalPinToTouchChannel(pin) > -1)
        //         {
        // #if defined(SOC_TOUCH_VERSION_1)
        //             val = touchRead(pin);
        //             val = -val;
        // #else
        //             val = touchRead(pin);
        // #endif
        //         }
        // #endif
        for (int i = 0; i < 24; i++)
        {

            pinMode(pin, INPUT_PULLDOWN);
            delayMicroseconds(1);

            pinMode(pin, INPUT_PULLUP);
            pinMode(pin, INPUT);
            val += analogRead(pin);

            pinMode(pin, INPUT_PULLDOWN);
            pinMode(pin, INPUT);
            val -= analogRead(pin);
        }
        pinMode(pin, INPUT_PULLDOWN);

        return -(val / 24);
    }

    static GPIOInfo *gpioByName(const std::string &name)
    {
        for (auto &i : gpioInfo)
        {
            if (strcmp(i->name, name.c_str()) == 0)
            {
                return i;
            }
        }
        return 0;
    }

    void declareInput(const std::string &name, int pin, int (*readFunction)(int))
    {
        auto p = gpioByName(name);
        if (!p)
        {
            p = new GPIOInfo();
            gpioInfo.push_back(p);
        }
        p->pin = pin;
        char *n = new char[name.size() + 1];
        strcpy(n, name.c_str());
        p->name = n;
        p->in = true;
        p->readFunction = readFunction;
    }

    void declareOutput(const std::string &name, int pin, void (*outputFunction)(int, int))
    {
        auto p = gpioByName(name);
        if (!p)
        {
            p = new GPIOInfo();
            gpioInfo.push_back(p);
        }
        p->pin = pin;
        char *n = new char[name.size() + 1];
        strcpy(n, name.c_str());
        p->name = n;
        p->out = true;
        p->writeFunction = outputFunction;
    }

    void markDangerous(const std::string &name)
    {
        auto p = gpioByName(name);
        if (p)
        {
            p->danger = true;
        }
        else
        {
            throw std::runtime_error("Unknown pin " + name);
        }
    }

    /// This thread manages GPIO inputs

    // Poll this every frame if we have non-interrupt driven stuff going on
    static void pollAllInputs()
    {
        for (auto &i : simpleInputs)
        {
            i.poll();
        }

        for (auto &i : analogInputs)
        {
            i.poll();
        }
    }

    static void checkAllInputs()
    {
        for (auto &i : simpleInputs)
        {
            i.testResistorPresence();
        }

        for (auto &i : analogInputs)
        {
            i.testResistorPresence();
        }
    }

    static void ARDUINO_ISR_ATTR wakeISR()
    {
        cogs::wakeMainThreadISR();
    }

    static void availableInputsAPI(AsyncWebServerRequest *request)
    {
        JsonDocument doc;
        doc["type"] = "string";
        int ctr = 0;
        for (const auto &e : gpioInfo)
        {
            if (e->in == true)
            {
                doc["enum"][ctr] = e->name;
                ctr++;
            }
        }

        char output[4096];
        serializeJson(doc, output, 4096);
        request->send(200, "application/json", output);
    }

    static void availableOutputsAPI(AsyncWebServerRequest *request)
    {
        JsonDocument doc;
        doc["type"] = "string";
        int ctr = 0;
        doc["enum"] = JsonArray();
        for (const auto &e : gpioInfo)
        {
            if (e->out == true)
            {
                doc["enum"][ctr] = e->name;
                ctr++;
            }
        }

        char output[4096];
        serializeJson(doc, output, 4096);
        request->send(200, "application/json", output);
    }

    static void gpioFromFile()
    {
        foundError = false;
        simpleInputs.clear();
        simpleOutputs.clear();
        analogInputs.clear();

        // Load from file
        JsonDocument doc;

        File file = LittleFS.open("/config/gpio.json", "r"); // flawfinder: ignore
        if (!file)
        {
            cogs::logInfo("No gpio.json found");
            return;
        }

        DeserializationError error = deserializeJson(doc, file);
        if (error)
        {
            foundError = true;
            cogs::logError(error.c_str());
            cogs::addTroubleCode("EGPIOCONFIG");
            return;
        }

        JsonVariant inputs = doc["inputs"];
        if (inputs.is<JsonArray>())
        {

            for (auto const &input : inputs.as<JsonArray>())
            {
                simpleInputs.emplace_back(input);
            }
        }

        JsonVariant outputs = doc["outputs"];
        if (outputs.is<JsonArray>())
        {

            for (auto const &output : outputs.as<JsonArray>())
            {
                simpleOutputs.emplace_back(output);
            }
        }

        JsonVariant analogs = doc["analogInputs"];
        if (analogs.is<JsonArray>())
        {

            for (auto const &analog : analogs.as<JsonArray>())
            {
                analogInputs.emplace_back(analog);
            }
        }

        if (foundError)
        {
            cogs::addTroubleCode("EGPIOCONFIG");
        }
        else
        {
            cogs::inactivateTroubleCode("EGPIOCONFIG");
        }
    }

    static void globalEventsHandler(cogs::GlobalEvent event, int dummy, const std::string &data)
    {

        if (event == cogs::fileChangeEvent)
        {
            if (data == "/config/gpio.json")
            {
                try
                {
                    gpioFromFile();
                }
                catch (const std::exception &e)
                {
                    cogs::logError(e.what());
                }
            }
        }
    }

    static bool alreadySetup = false;
    void begin()
    {
        if (alreadySetup)
        {
            cogs::logInfo("Already setup gpio app");
            return;
        }

        // if (cogs_editable_automation::started)
        // {
        //     cogs::logError("gpio should start before rules");
        //     cogs::addTroubleCode("ESETUPORDER");
        // }

        alreadySetup = true;
        cogs::globalEventHandlers.push_back(globalEventsHandler);
        cogs::registerFastPollHandler(pollAllInputs);
        cogs_web::server.on("/builtin/schemas/gpio.json", HTTP_GET, [](AsyncWebServerRequest *request)
                            { cogs_web::sendGzipFile(request, gpio_schema_json_gz, sizeof(gpio_schema_json_gz), "application/json"); });
        cogs_web::NavBarEntry::create("🔌GPIO", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/gpio.json&filename=/config/gpio.json");

        // We use dynamically generated schemas for the pins
        cogs_web::server.on("/gpio/input_pin_schema.json", HTTP_GET, availableInputsAPI);
        cogs_web::server.on("/gpio/output_pin_schema.json", HTTP_GET, availableOutputsAPI);

        try
        {
            gpioFromFile();
        }
        catch (const std::exception &e)
        {
            cogs::logError(e.what());
        }
    }

    void onSourceTagSet(cogs_rules::IntTagPoint *tag)
    {
        CogsSimpleOutput *o = reinterpret_cast<CogsSimpleOutput *>(tag->extraData);
        if (o == nullptr)
        {
            return;
        }

        int v = tag->value[0];

        if (v >= o->pwmSteps)
        {
            v = o->pwmSteps;
        }

        if (o->invert)
        {
            v = o->pwmSteps - v;
        }

        if (o->writeFunction)
        {
            o->writeFunction(o->pin, tag->value[0]);
        }
        else
        {
            if (o->pwmSteps > 1)
            {
                analogWrite(o->pin, v);
            }
            else
            {
                digitalWrite(o->pin, v > 0 ? HIGH : LOW);
            }
        }
    }

    CogsSimpleOutput::CogsSimpleOutput(const JsonVariant &config)
    {

        if (!config.is<JsonObject>())
        {
            this->pin = -1;
            return;
        }

        std::string pinName = config["pin"].as<std::string>();
        const cogs_gpio::GPIOInfo *p = gpioByName(pinName);
        if (p == 0)
        {
            this->pin = -1;
            return;
        }

        int pin = p->pin;

        this->pin = pin;

        cogs::logInfo("Using output " + pinName + " at " + std::to_string(pin));

        if (config["pwmSteps"].is<int>())
        {
            this->pwmSteps = config["pwmSteps"].as<int>();
        }

        if (config["invert"].is<bool>())
        {
            this->invert = config["invert"].as<bool>();
        }
        std::string st = config["source"].as<std::string>();

        // We need to "own" the tag and can't use an existing one,
        // otherwise someone else might mess with extra data
        // so enforce a prefix
        if (!(st.rfind("outputs.", 0) == 0))
        {
            st = "outputs." + st;
        }

        this->sourceTag = cogs_rules::IntTagPoint::getTag(st, 0, 1, cogs_rules::FXP_RES);
        this->sourceTag->extraData = this;
        this->sourceTag->subscribe(&onSourceTagSet);

        if (pin < 1024)
        {
            pinMode(pin, OUTPUT);
        }
        onSourceTagSet(this->sourceTag.get());
    }

    CogsSimpleOutput::~CogsSimpleOutput()
    {
        if (this->pin == -1)
        {
            return;
        }
        if (this->sourceTag)
        {
            this->sourceTag->unsubscribe(&onSourceTagSet);
        }

        if (this->pin < 1024)
        {
            pinMode(this->pin, INPUT);
        }
    }

    void CogsDigitalInput::configureDigitalPropsFromData(const JsonVariant &config)
    {
        if (config["activeHigh"].is<bool>())
        {
            this->activeHigh = config["activeHigh"].as<bool>();
        }

        if (config["debounce"].is<int>())
        {
            this->debounce = config["debounce"].as<int>();
        }

        if (config["openDrainPilotResistor"].is<bool>())
        {
            this->needPilotResistor = config["openDrainPilotResistor"].as<bool>();
        }
    }

    bool CogsDigitalInput::testResistorPresence()
    {

        if (!this->needPilotResistor)
        {
            return true;
        }

        if (this->pin >= 1024)
        {
            return true;
        }
        pinMode(this->pin, INPUT_PULLUP);
        delayMicroseconds(40);
        pinMode(this->pin, INPUT);
        delay(1);

        if (!digitalRead(this->pin))
        {
            if (!this->pilotResistorPresent)
            {
                this->pilotResistorPresent = true;
                cogs::inactivateTroubleCode("EINPUTDISCONNECTED");
            }
            return true;
        }

        if (this->pilotResistorPresent)
        {

            this->pilotResistorPresent = false;
            cogs::addTroubleCode("EINPUTDISCONNECTED");
        }
        return false;
    }

    CogsSimpleInput::CogsSimpleInput(const JsonVariant &config)
    {

        if (!config.is<JsonObject>())
        {
            throw std::runtime_error("Invalid config");
        }

        this->configureDigitalPropsFromData(config);

        std::string pinName = config["pin"].as<std::string>();

        if (config["pullup"].is<bool>())
        {
            this->pullup = config["pullup"].as<bool>();
        }

        auto p = gpioByName(pinName);
        if (!p)
        {
            throw std::runtime_error("Pin " + pinName + " not found");
        }

        unsigned int pin = p->pin;
        this->pin = pin;

        cogs::logInfo("Using input " + pinName + " at " + std::to_string(pin));

        if (pin < 1024)
        {
            if (this->pullup || pullupEnabledOnPin(pin))
            {
                pinMode(pin, INPUT_PULLUP);
#if defined(ESP32) || defined(ESP8266)

                rtc_gpio_pullup_en((gpio_num_t)pin);
                rtc_gpio_pulldown_dis((gpio_num_t)pin);
#endif

#if defined(ESP32)
                gpio_sleep_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
#endif
                cogs::logInfo("Using pullup");
            }
            else
            {
                cogs::logInfo("No pullup");
                pinMode(pin, INPUT);
            }
        }

        this->setupDigitalTargetsFromJson(config);

        this->lastInputLevel = digitalRead(pin);

        if (this->digitalValueTarget)
        {
            this->digitalValueTarget->setValue(this->lastInputLevel == this->activeHigh, 0, 1);
        }

        int interuptNumber = -1;
        if (pin < 1024)
        {

#if defined(ESP32) || defined(ESP8266)

            if (config["deepSleepWake"].is<bool>())
            {
                if (config["deepSleepWake"].as<bool>())
                {
                    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, this->activeHigh);

                    if (pullup)
                    {
                        rtc_gpio_pullup_en((gpio_num_t)pin);
                        rtc_gpio_pulldown_dis((gpio_num_t)pin);
                    }
                }
            }
#endif
            interuptNumber = digitalPinToInterrupt(pin);
        }
        if (interuptNumber > -1)
        {
            attachInterrupt(interuptNumber, wakeISR, CHANGE);
        }
    }

    void CogsDigitalInput::setupDigitalTargetsFromJson(const JsonVariant &config)
    {
        if (config["activeTarget"].is<const char *>())
        {
            std::string st = config["activeTarget"].as<std::string>();

            if (!(st.rfind("inputs.", 0) == 0))
            {
                st = "inputs." + st;
            }

            if (st.size() > 0)
            {
                this->activeTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
            }
        }

        if (config["inactiveTarget"].is<const char *>())
        {

            std::string st = config["inactiveTarget"].as<std::string>();
            if (!(st.rfind("inputs.", 0) == 0))
            {
                st = "inputs." + st;
            }

            if (st.size() > 0)
            {
                this->inactiveTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
            }
        }

        if (config["digitalValueTarget"].is<const char *>())
        {
            std::string st = config["digitalValueTarget"].as<std::string>();
            if (!(st.rfind("inputs.", 0) == 0))
            {
                st = "inputs." + st;
            }

            if (st.size() > 0)
            {
                this->digitalValueTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
                this->digitalValueTarget->setUnit("bool");
            }
        }
    }

    CogsSimpleInput::~CogsSimpleInput()
    {
        int interuptNumber = -1;
        if (this->pin < 1024)
        {
            interuptNumber = digitalPinToInterrupt(this->pin);
        }
        if (interuptNumber > -1)
        {
            detachInterrupt(interuptNumber);
        }
    }

    void CogsSimpleInput::poll()
    {
        if (this->pin == -1)
        {
            return;
        }
        if (millis() - this->debounceTimestamp < this->debounce)
        {
            return;
        }

        bool val;
        if (this->readFunction == nullptr)
        {
            val = digitalRead(this->pin);
        }
        else
        {
            val = this->readFunction(this->pin);
        }

        this->onNewRawDigitalValue(val);
    }

    void CogsDigitalInput::onNewRawDigitalValue(bool val)
    {
        if (val != this->lastInputLevel)
        {
            this->debounceTimestamp = millis();

            if (val == this->activeHigh)
            {
                if (this->activeTarget)
                {
                    int v = cogs::bang(this->activeTarget->value[0]);
                    this->activeTarget->setValue(v, 0, 1);
                }
            }
            else
            {
                if (this->inactiveTarget)
                {
                    int v = cogs::bang(this->inactiveTarget->value[0]);
                    this->inactiveTarget->setValue(v, 0, 1);
                }
            }

            if (this->digitalValueTarget)
            {
                this->digitalValueTarget->setValue(val == this->activeHigh, 0, 1);
            }

            this->lastInputLevel = val;
        }
    }

    CogsAnalogInput::CogsAnalogInput(const JsonVariant &config)
    {
        if (!config.is<JsonObject>())
        {
            throw std::runtime_error("Invalid config");
        }

        this->configureDigitalPropsFromData(config);

        std::string pinName = config["pin"].as<std::string>();

        if (config["pullup"].is<bool>())
        {
            this->pullup = config["pullup"].as<bool>();
        }

        if (config["referencePin"].is<const char *>())
        {
            auto r = gpioByName(config["referencePin"].as<std::string>());

            if (r)
            {
                if (!r->danger)
                {
                    this->referencePin = r->pin;
                    this->referenceWriteFunction = r->writeFunction;
                }
                else
                {
                    cogs::logError("Cannot use dangerous pin as reference");
                    foundError = true;
                }
            }
            else
            {
                cogs::logError("Invalid reference pin " + config["referencePin"].as<std::string>());
            }
        }

        std::string scale = "1";

        if (config["scale"].is<const char *>())
        {
            scale = config["scale"].as<std::string>();
        }

        this->rawToScaledMultiplier = cogs_rules::evalExpression(scale);
        if (this->rawToScaledMultiplier == 0)
        {
            cogs::logError("Invalid scale " + scale);
            this->rawToScaledMultiplier = 1;
        }

        if (config["analogValueTarget"].is<const char *>())
        {
            std::string st = config["analogValueTarget"].as<std::string>();

            if (!(st.rfind("inputs.", 0) == 0))
            {
                st = "inputs." + st;
            }

            if (st.size() > 0)
            {
                this->analogValueTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1, cogs_rules::FXP_RES);
            }
        }

        auto p = gpioByName(pinName);
        if (!p)
        {
            throw std::runtime_error("Pin " + pinName + " not found");
        }

        unsigned int pin = p->pin;
        this->pin = pin;

        cogs::logInfo("Using analog ch " + pinName + " at " + std::to_string(pin));

        if (pin < 1024)
        {

            this->channel = pin;
            if (this->pullup || pullupEnabledOnPin(pin))
            {
                pinMode(pin, INPUT_PULLUP);
#if defined(ESP32) || defined(ESP8266)

                rtc_gpio_pullup_en((gpio_num_t)pin);
                rtc_gpio_pulldown_dis((gpio_num_t)pin);

#endif
#if defined(ESP32)
                gpio_sleep_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
#endif
                cogs::logInfo("Using pullup");
            }
            else
            {
                cogs::logInfo("No pullup");
                pinMode(pin, INPUT);
            }
        }

        if (config["capSense"].is<bool>())
        {
            if (config["capSense"].as<bool>())
            {
                if (this->pin > 1024)
                {
                    cogs::logError("Cannot use capsense on virtual pin");
                    this->singleEndedSense = false;
                    foundError = true;
                }
                else
                {
                    cogs::logInfo("Using sw capsense");
                    this->readFunction = &swCapsenseRead;
                }
            }
        }

        if (this->referencePin > -1)
        {
            if (!this->referenceWriteFunction)
            {
                pinMode(this->referencePin, OUTPUT);
            }
        }

        this->valMult = this->rawToScaledMultiplier * cogs_rules::FXP_RES;

        if (config["zeroValue"].is<double>())
        {
            this->rawUnitCenterValue = config["zeroValue"].as<double>() / this->rawToScaledMultiplier;
        }

        this->setupDigitalTargetsFromJson(config);

        int samples = 64;

        // Assume virtual pins are slow.
        if (this->readFunction)
        {
            samples = 8;
        }

        int avg = 0;
        for (int i = 0; i < samples; i++)
        {
            avg += this->rawRead();
        }

        avg /= samples;

        // Automatic zero at boot, only if it's fairly close to the center
        if (config["autoZero"].as<float>())
        {

            this->rawUnitMaxAutoZero = config["autoZero"].as<float>() / this->rawToScaledMultiplier;

            if ((avg > this->rawUnitCenterValue + (this->rawUnitMaxAutoZero)) ||
                (avg < this->rawUnitCenterValue - (this->rawUnitMaxAutoZero)))
            {
                cogs::addTroubleCode("EAUTOZEROFAIL");
            }
            else
            {
                this->rawUnitCenterValue = avg;
            }
        }

        int interuptNumber = -1;
        if (pin < 1024)
        {

#if defined(ESP32) || defined(ESP8266)
            if (config["deepSleepWake"].is<bool>())
            {
                if (config["deepSleepWake"].as<bool>())
                {
                    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin, this->activeHigh);
                }
            }
#endif
            interuptNumber = digitalPinToInterrupt(pin);
        }
        if (interuptNumber > -1)
        {
            attachInterrupt(interuptNumber, wakeISR, CHANGE);
        }
    }

    inline int CogsAnalogInput::rawRead()
    {
        // If we have a reference pin, we actually take two measurements,
        // one with the reference pin high and one with it low,
        // and subtract them.  This gives a lock in amplifier style
        // measurement.

        int val = 0;
        if (this->referencePin > -1)
        {
            if (this->referenceWriteFunction)
            {
                this->referenceWriteFunction(this->referencePin, 1);
            }
            else
            {
                digitalWrite(this->referencePin, HIGH);
            }
        }

        if (this->readFunction == nullptr)
        {
            // TODO this is awful hack because analog read undoes pullups
            val = ANREAD(this->pin);
            if (this->pullup)
            {
                pinMode(this->pin, INPUT_PULLUP);
            }
            else
            {
                pinMode(this->pin, INPUT);
            }
        }
        else
        {
            val = this->readFunction(this->pin);
        }

        if (this->referencePin > -1)
        {
            if (this->referenceWriteFunction)
            {
                this->referenceWriteFunction(this->referencePin, 0);
            }
            else
            {
                digitalWrite(this->referencePin, LOW);
            }
            if (this->readFunction == nullptr)
            {
                val -= ANREAD(this->pin);

                if (this->pullup)
                {
                    pinMode(this->pin, INPUT_PULLUP);
                }
                else
                {
                    pinMode(this->pin, INPUT);
                }
            }
            else
            {
                val -= this->readFunction(this->pin);
            }
        }

        return val;
    }

    void CogsAnalogInput::poll()
    {
        // unsigned long st = micros();

        if (this->pin == -1)
        {
            return;
        }

        int val = this->rawRead();

        float realMeasurement = (val - this->rawUnitCenterValue);
        if (this->analogValueTarget)
        {
            // Need to convert to the target scale
            this->analogValueTarget->setValue(realMeasurement * this->valMult, 0, 1);
        }
        // Serial.println(micros() - st);
    }
}