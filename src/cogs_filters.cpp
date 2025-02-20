#pragma once
#include <string>
#include "cogs_util.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include "cogs_filters.h"
#include "cogs_rules.h"
#include "cogs_trouble_codes.h"


using namespace cogs_rules;

cogs_rules::Filter::Filter() {}
cogs_rules::Filter::~Filter() {}
cogs_rules::Filter::Filter(int num, int resolution, const JsonObject &json) {};
bool cogs_rules::Filter::sample(int *input, int *current_target_vals) { return true; };
bool cogs_rules::Filter::handleEngineRefresh() { return true; };

void cogs_rules::Filter::setStateEnterTargetValue(int *input) {};

class ExpressionFilter : public cogs_rules::Filter
{
private:
    std::string expression;
    te_expr *expr = 0;
    int count;
    float scale;
    float scale_inv;

public:
    ExpressionFilter(int num, int resolution, const JsonObject &json)
    {
        count = num;
        expression = json["expression"].as<std::string>();
        this->expr = cogs_rules::compileExpression(expression);
        this->scale = resolution;
        this->scale_inv = 1.0f/resolution;
        if (!expr)
        {
            cogs::logError("Failed to compile filter expression: " + expression);
            cogs::addTroubleCode("EBADAUTOMATION");
        }
    };

    ~ExpressionFilter()
    {
        if (expr)
        {
            cogs_rules::freeExpression(expr);
        }
    }

    bool handleEngineRefresh()
    {
        if (expr)
        {
            cogs_rules::freeExpression(expr);
        }
        expr = cogs_rules::compileExpression(expression);
        if (expr)
        {
            return true;
        }

        return false;
    }

    bool sample(int *input, int *current_target_vals)
    {
        if (!expr)
        {
            return false;
        }
        for (int i = 0; i < count; i++)
        {
            dollar_sign_old = input[i]*this->scale_inv;
            input[i] = te_eval(expr) * this->scale;
        }

        return true;
    };
};


 class FadeInFilter : public cogs_rules::Filter
{
public:
    float time = 1000;
    int count = 0;
    float *state = nullptr;
    int32_t start = 0;

    inline FadeInFilter(int num, int resolution, const JsonObject &json)
    {
        if (json["time"].is<float>())
        {
            time = json["time"].as<float>();
        }
        this->state = new float[num];
        this->count = num;
        this->freeRun = true;
        start = millis();
    }

    // Tell filter about the value of the target when the state first enters.
    void setStateEnterTargetValue(int *input)
    {
        for (int i = 0; i < count; i++)
        {
            state[i] = input[i];
        }
        start = millis();
        freeRun = true;
    }

    bool sample(int *input, int *current_target_vals)
    {
        if (!freeRun)
        {
            return true;
        }

        int32_t now = millis();
        float delta = now - start;

        float blend = delta / time;
        if (blend >= 1.0)
        {
            blend = 1.0;
            freeRun = false;
            return true;
        }

        for (int i = 0; i < count; i++)
        {
            state[i] = blend * ((float)input[i]) + (1.0f - blend) * state[i];
            input[i] = (int)state[i];
        }
        return true;
        // Too some optimization thing
    }

    ~FadeInFilter()
    {
        delete[] state;
    }
};

class LowPassFilter : public cogs_rules::Filter
{
public:
    float timeConstant;
    int count = 0;
    float *state = nullptr;
    int32_t lastSample = 0;

    inline LowPassFilter(int num, int resolution, const JsonObject &json)
    {
        if (json["timeConstant"].is<float>())
        {
            timeConstant = json["timeConstant"].as<float>() / 1000.0f;
        }
        else
        {
            timeConstant = 0.01;
        }

        this->state = new float[num];
        this->count = num;
        this->freeRun = true;
        lastSample = millis();
    }

    // Tell filter about the value of the target when the state first enters.
    void setStateEnterTargetValue(int *input)
    {
        for (int i = 0; i < count; i++)
        {
            state[i] = input[i];
        }
    }

    bool sample(int *input, int *current_target_vals)
    {
        int32_t now = millis();
        int32_t delta = now - lastSample;
        float hz = 1000.0f / delta;
        lastSample = now;
        float blend = cogs::fastGetApproxFilterBlendConstant(timeConstant, hz);
        for (int i = 0; i < count; i++)
        {
            state[i] = (1.0f - blend) * ((float)input[i]) + blend * state[i];
            input[i] = (int)state[i];
        }
        return true;
        // Too some optimization thing
    }

    ~LowPassFilter()
    {
        delete[] state;
    }
};
/*
Drop samples that have not changed.  The first value when the filter starts,
which will be when the state enters, will not register as a change.
*/
class OnChangeFilter : public Filter
{
private:
    /* data */
    bool unknownState;
    int *lastState;
    int count;

public:
    OnChangeFilter(int num, int resolution, const JsonObject &json)
    {
        this->unknownState = new bool[num];
        this->lastState = new int[num];
        this->count = num;
        unknownState = true;
    }

    bool sample(int *input, int *current_target_vals)
    {
        if (this->unknownState)
        {
            memcpy(lastState, input, sizeof(int) * count);
            this->unknownState = false;
            return false;
        }
        bool changed = false;

        for (int i = 0; i < count; i++)
        {
            if (lastState[i] != input[i])
            {
                lastState[i] = input[i];
                changed = true;
            }
        }
        return changed;
    }

    void setStateEnterTargetValue(int *input)
    {
        this->unknownState = true;
    }
    ~OnChangeFilter()
    {
        delete[] lastState;
    }
};

/*
Pass through trigger
*/
class TriggerFilter : public Filter
{
private:
    /* data */
    bool unknownState;
    int *lastState;
    int count;
    int resolution;

public:
    TriggerFilter(int num, int resolution, const JsonObject &json)
    {
        this->unknownState = true;
        this->lastState = new int[num];
        this->count = num;
        this->resolution = resolution;
    }

    bool sample(int *input, int *current_target_vals)
    {
        if (this->unknownState)
        {
            memcpy(lastState, input, sizeof(int) * count);
            this->unknownState = false;
            return false;
        }
        bool changed = false;

        for (int i = 0; i < count; i++)
        {
            if (lastState[i] != input[i])
            {
                lastState[i] = input[i];
                if (input[i] != 0)
                {
                    changed = true;
                    input[i] = cogs::bang(current_target_vals[i] / this->resolution) * this->resolution;
                }
            }
        }
        return changed;
    }

    void setStateEnterTargetValue(int *input)
    {
        this->unknownState = true;
    }
    ~TriggerFilter()
    {
        delete[] lastState;
    }
};

Filter *cogs_rules::createFilter(int num, int resolution, const JsonObject &json)
{

    std::string type = json["type"].as<std::string>();

    cogs::logInfo("Creating filter: " + type);

    if (type == "null")
    {
        return new Filter();
    }

    else if (type == "change")
    {
        return new OnChangeFilter(num, resolution, json);
    }
    else if (type == "trigger")
    {
        return new TriggerFilter(num, resolution, json);
    }

    else if (type == "lowpass")
    {
        return new LowPassFilter(num, resolution, json);
    }

    else if (type == "fadeIn")
    {
        return new FadeInFilter(num, resolution, json);
    }

    else if (type == "expression")
    {
        return new ExpressionFilter(num, resolution, json);
    }

    cogs::logError("Unknown filter type: " + type);
    return nullptr;
}
