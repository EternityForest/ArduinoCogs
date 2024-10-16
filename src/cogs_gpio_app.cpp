#include "cogs_gpio_app.h"
#include "cogs_global_events.h"
#include "cogs_rules.h"
#include "web/cogs_web.h"
#include "web/generated_data/gpio_schema_json_gz.h"
#if defined(ESP32) || defined(ESP8266)
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#endif
#include <LittleFS.h>
#include <Arduino.h>

class GPIOInfo
{
public:
    int pin;
    char *name;
    bool in;
    bool out;
};

namespace cogs_gpio
{
    std::vector<GPIOInfo *> gpioInfo;

    std::vector<CogsSimpleInput> simpleInputs;
    std::vector<CogsSimpleOutput> simpleOutputs;

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

    void declareInput(const std::string &name, int pin)
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
    }

    void declareOutput(const std::string &name, int pin)
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
    }

    /// This thread manages GPIO inputs

    // Poll this every frame if we have non-interrupt driven stuff going on
    static void pollAllSimpleInputs()
    {
        for (auto &i : simpleInputs)
        {
            i.poll();
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
        for (auto const &e : gpioInfo)
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
        for (auto const &e : gpioInfo)
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
        simpleInputs.clear();
        simpleOutputs.clear();

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
            throw std::runtime_error(error.c_str());
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
        if (inputs.is<JsonArray>())
        {

            for (auto const &output : outputs.as<JsonArray>())
            {
                simpleOutputs.emplace_back(output);
            }
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
        alreadySetup = true;
        cogs::globalEventHandlers.push_back(globalEventsHandler);
        cogs::registerFastPollHandler(pollAllSimpleInputs);
        cogs_web::server.on("/builtin/schemas/gpio.json", HTTP_GET, [](AsyncWebServerRequest *request)
                            { cogs_web::sendGzipFile(request, gpio_schema_json_gz, sizeof(gpio_schema_json_gz), "application/json"); });
        cogs_web::NavBarEntry::create("GPIO", "/default-template?load-module=/builtin/jsoneditor_app.js&schema=/builtin/schemas/gpio.json&filename=/config/gpio.json");

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
            o->writeFunction(tag->value[0]);
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
            throw std::runtime_error("Invalid config");
        }

        std::string pinName = config["pin"].as<std::string>();
        auto p = gpioByName(pinName);
        if (p == 0)
        {
            throw std::runtime_error("Invalid pin " + pinName);
        }

        int pin = p->pin;

        cogs::logInfo("Using output " + pinName + " at " + std::to_string(pin));

        pinMode(pin, OUTPUT);

        std::string st = config["source"].as<std::string>();

        this->sourceTag = cogs_rules::IntTagPoint::getTag(st, 0);
        this->sourceTag->extraData = this;
        this->sourceTag->subscribe(&onSourceTagSet);
    }

    CogsSimpleOutput::~CogsSimpleOutput()
    {
        if (this->sourceTag)
        {
            this->sourceTag->unsubscribe(&onSourceTagSet);
        }
    }
    CogsSimpleInput::CogsSimpleInput(const JsonVariant &config)
    {

        if (!config.is<JsonObject>())
        {
            throw std::runtime_error("Invalid config");
        }

        std::string pinName = config["pin"].as<std::string>();

        bool pullup = false;
        if (config["pullup"].is<bool>())
        {
            pullup = config["pullup"].as<bool>();
        }

        auto p = gpioByName(pinName);
        if (!p)
        {
            throw std::runtime_error("Pin " + pinName + " not found");
        }

        unsigned int pin = p->pin;

        cogs::logInfo("Using input" + pinName + " at " + std::to_string(pin));

        if (pullup)
        {
            pinMode(pin, INPUT_PULLUP);
        }
        else
        {
            pinMode(pin, INPUT);
        }
        this->lastInputLevel = digitalRead(pin);

        this->pin = pin;

        int interuptNumber = -1;
        if (pin < 1024)
        {
            interuptNumber = digitalPinToInterrupt(pin);
        }
        if (interuptNumber > -1)
        {
            attachInterrupt(interuptNumber, wakeISR, CHANGE);
        }

        if (config["activeHigh"].is<bool>())
        {
            this->activeHigh = config["activeHigh"].as<bool>();
        }

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

        this->setupTargetsFromJson(config);
    }

    void CogsSimpleInput::setupTargetsFromJson(const JsonVariant &config)
    {
        if (config["activeTarget"].is<const char *>())
        {
            std::string st = config["activeTarget"].as<std::string>();
            if (st.size() > 0)
            {
                this->activeTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
            }
        }

        if (config["inactiveTarget"].is<const char *>())
        {
            std::string st = config["inactiveTarget"].as<std::string>();
            if (st.size() > 0)
            {
                this->inactiveTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
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
        bool val;
        if (this->readFunction == nullptr)
        {
            val = digitalRead(this->pin);
        }
        else
        {
            val = this->readFunction();
        }

        this->onNewRawDigitalValue(val);
    }

    void CogsSimpleInput::onNewRawDigitalValue(bool val)
    {
        if (val != this->lastInputLevel)
        {
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
            this->lastInputLevel = val;
        }
    }

}