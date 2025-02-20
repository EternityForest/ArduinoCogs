#include "tagpoint_template.h"
/// This file defines generic tag points, but note that almost all features use numeric fixed point tags.
using namespace cogs_tagpoints;




TagPoint::TagPoint(const std::string &n, TAG_DATA_TYPE val, uint16_t count)
{
    this->name = n;
    this->count = count;
    // Dynamic allocation because we don't know how big this will be.
    this->background_value = reinterpret_cast<TAG_DATA_TYPE *>(malloc(sizeof(TAG_DATA_TYPE) * count));
    this->value = reinterpret_cast<TAG_DATA_TYPE *>(malloc(sizeof(TAG_DATA_TYPE) * count));

    // Set the value to be all the same
    for (int i = 0; i < count; i++)
    {
        this->background_value[i] = val;
    }
    this->rerender();
}

TagPoint::~TagPoint()
{
    if (this->background_value)
    {
        free(this->background_value);
    }

    if (this->value)
    {
        free(this->value);
    }
}

//! Unregister a tag. It should not be used after that.
void TagPoint::unregister()
{
    for (auto tag = TagPoint::all_tags.begin(); tag != TagPoint::all_tags.end(); tag++)
    {
        if ((*tag)->name == name)
        {
            TagPoint::all_tags.erase(tag);
            cogs::triggerGlobalEvent(cogs::tagDestroyedEvent, 0, name);
            break;
        }
    }
}



void TagPoint::subscribe(void (*func)(TagPoint *))
{
    // Defensive null crash blocking
    if (func)
    {
        // Ensure idempotency
        this->unsubscribe(func);
        this->subscribers.push_back(func);
    }
};

void TagPoint::unsubscribe(void (*func)(TagPoint *))
{
    for (auto it = this->subscribers.begin(); it != this->subscribers.end(); it++)
    {
        if (*it == func)
        {
            this->subscribers.erase(it);
            break;
        }
    }
};

void TagPoint::setValue(TAG_DATA_TYPE val, uint16_t startIndex, uint16_t count)
{
    if (count == 0)
    {
        count = this->count;
    }


    int upTo = startIndex + count;

    for (int i = startIndex; i < upTo; i++)
    {
        this->background_value[i] = val;
    }

    // Render makes the background values real if needed and notifies subscribers
    // If the rendered value changes.
    this->rerender();
};

void TagPoint::smartSetValue(TAG_DATA_TYPE val, int minDifference, int interval)
{
    bool x = false;

    if (val > this->background_value[0])
    {
        if (val - this->background_value[0] >= minDifference)
        {
            x = true;
        }
    }
    else
    {
        if (this->background_value[0] - val >= minDifference)
        {
            x = true;
        }
    }

    if (!x)
    {
        if (millis() - this->lastChangeTime >= interval)
        {
            x = true;
        }
    }
    if (x)
    {
        this->setValue(val);
    }
}

/// Set all vals to 0 
/// Do not trigger subscribers
/// Call from within a "bang" type one shot handler
/// If you want to make it so setting it to 1 again triggers it again.

void TagPoint::silentResetValue()
{
    for (int i = 0; i < this->count; i++)
    {
        this->background_value[i] = 0;
    }
}


void TagPoint::rerender()
{
        if (memcmp(this->value, this->background_value, sizeof(TAG_DATA_TYPE) * this->count) != 0)
        {
            // Only first value changes affect ws api
            if (this->value[0] != this->background_value[0])
            {
                this->webAPIDirty = true;
            }

            memcpy(this->value, this->background_value, sizeof(TAG_DATA_TYPE) * this->count); // flawfinder: ignore

            this->floatFirstValueCache = ((float)this->value[0]) * this->scale_inverse;

            this->notifySubscribers();
        }
};

void TagPoint::notifySubscribers()
{
    // Push data to subscribers
    for (auto const &func : this->subscribers)
    {
        func(this);
    }
    // The whole point of this is that it happens after the subscribers have been notified
    // don't move it up!
    this->lastChangeTime = millis();
}
