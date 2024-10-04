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
    std::map<int, void (*)(bool)> outputFunctions;

    std::vector<CogsSimpleInput> simpleInputs;

    void declareInput(const std::string &name, int pin){
        availableInputs[name] = pin;
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
        if (!inputs.is<JsonArray>())
        {
            throw std::runtime_error("No inputs in gpio.json");
        }

        for (auto const &input : inputs.as<JsonArray>())
        {
            simpleInputs.emplace_back(input);
        }
    }

    static void globalEventsHandler(cogs::GlobalEvent event, int dummy, const std::string &data)
    {

        if (event == cogs::fileChangeEvent)
        {
            if (data == "/config/gpio.json")
            {
                gpioFromFile();
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

        gpioFromFile();
    }

    CogsSimpleInput::CogsSimpleInput(const JsonVariant &config)
    {

        if(!config.is<JsonObject>()){
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

        if (config.containsKey("activeTarget"))
        {
            this->activeTarget = cogs_rules::IntTagPoint::getTag(config["activeHigh"].as<std::string>(), 0, 1);
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