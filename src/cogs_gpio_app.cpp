#include "cogs_gpio_app.h"
#include "cogs_global_events.h"
#include "cogs_bindings_engine.h"
#include "web/cogs_web.h"
#include "web/generated_data/gpio_schema_json_gz.h"

#include <LittleFS.h>
#include <Arduino.h>

namespace cogs_gpio
{
    std::map<std::string, int> availableInputs;
    std::map<std::string, int> availableOutputs;
    std::map<int, bool (*)()> inputFunctions;
    std::map<int, void (*)(int)> outputFunctions;

    std::vector<CogsSimpleInput> simpleInputs;
    std::vector<CogsSimpleOutput> simpleOutputs;

    void declareInput(const std::string &name, int pin)
    {
        availableInputs[name] = pin;
    }

    void declareOutput(const std::string &name, int pin)
    {
        availableOutputs[name] = pin;
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

    static void wakeISR()
    {
        // just used to wake up.
    }

    static void availableInputsAPI(AsyncWebServerRequest *request)
    {
        JsonDocument doc;
        doc["type"] = "string";
        int ctr = 0;
        for (auto const &e : availableInputs)
        {
            doc["enum"][ctr] = e.first;
            ctr++;
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
        for (auto const &e : availableOutputs)
        {
            doc["enum"][ctr] = e.first;
            ctr++;
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

    void begin()
    {
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
        int pin = availableOutputs[pinName];
        cogs::logInfo("Using output " + pinName + " at " + std::to_string(pin));

        if (outputFunctions.contains(pin))
        {
            this->writeFunction = outputFunctions[pin];
        }
        else
        {
            pinMode(pin, OUTPUT);
        }

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
        if (config.containsKey("pullup"))
        {
            pullup = config["pullup"].as<bool>();
        }

        if (!availableInputs.contains(pinName))
        {
            throw std::runtime_error("Pin " + pinName + " not found");
        }

        int pin = availableInputs[pinName];

        cogs::logInfo("Using input" + pinName + " at " + std::to_string(pin));

        if (inputFunctions.contains(pin))
        {
            this->readFunction = inputFunctions[pin];
            this->lastInputLevel = this->readFunction();
            cogs::registerFastPollHandler(pollAllSimpleInputs);
        }
        else
        {
            if (pullup)
            {
                pinMode(pin, INPUT_PULLUP);
            }
            else
            {
                pinMode(pin, INPUT);
            }
            this->lastInputLevel = digitalRead(pin);
        }

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

        if (config.containsKey("activeHigh"))
        {
            this->activeHigh = config["activeHigh"].as<bool>();
        }

        this->setupTargetsFromJson(config);
    }

    void CogsSimpleInput::setupTargetsFromJson(const JsonVariant &config)
    {
        if (config.containsKey("activeTarget"))
        {
            std::string st = config["activeTarget"].as<std::string>();
            if (st.size() > 0)
            {
                this->activeTarget = cogs_rules::IntTagPoint::getTag(st, 0, 1);
            }
        }

        if (config.containsKey("inactiveTarget"))
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
            if (this->activeTarget)
            {
                int v = cogs::bang(this->activeTarget->value[0]);
                this->activeTarget->setValue(v, 0, 1);
            }
            if (this->inactiveTarget)
            {
                int v = cogs::bang(this->inactiveTarget->value[0]);
                this->inactiveTarget->setValue(v, 0, 1);
            }
            this->lastInputLevel = val;
        }
    }

}