#include "tagpoint_template.h"
/// This file defines generic tag points, but note that almost all features use numeric fixed point tags.
using namespace cogs_tagpoints;

static bool claimCmpGreater(const std::shared_ptr<TagPointClaim> a, const std::shared_ptr<TagPointClaim> b)
{
    return a->priority < b->priority;
}

TagPointClaim::TagPointClaim(uint16_t startIndex, uint16_t count)
{

    this->finished = false;
    this->startIndex = startIndex;
    this->value = (TAG_DATA_TYPE *)malloc(sizeof(TAG_DATA_TYPE) * count);
    if (this->value == nullptr)
    {
        throw std::runtime_error("malloc failed: " + std::to_string(count));
    }

    for (int i = 0; i < count; i++)
    {
        this->value[i] = 0;
    }
    this->count = count;
};

TagPointClaim::~TagPointClaim()
{

    if (this->value != nullptr)
    {
        free(this->value);
    }
};

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
        free(this->background_value);
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

void TagPoint::cleanFinished()
{
    bool unfinished_passed = false;

    // How many finished ones are at the start.
    // We can delete these after we are done with them.
    int finished_count = 0;

    for (const auto &claim : this->claims)
    {
        if (!unfinished_passed)
        {
            if (claim->finished)
            {
                finished_count += 1;
            }
        }
        else
        {
            unfinished_passed = true;
        }
    }
    while (finished_count)
    {
        this->claims.erase(this->claims.begin());
        finished_count--;
    }
};

std::shared_ptr<TagPointClaim> TagPoint::overrideClaim(uint16_t layer, TAG_DATA_TYPE value, uint16_t startIndex, uint16_t count)
{
    TagPointClaim *c = new TagPointClaim(startIndex, count);
    c->startIndex = startIndex;
    std::shared_ptr<TagPointClaim> p(c);

    for (int i = 0; i < count; i++)
    {
        c->value[i] = value;
    }
    c->priority = layer;

    this->claims[layer] = p;
    std::sort(this->claims.begin(), this->claims.end(), claimCmpGreater);

    return p;
}

void TagPoint::removeClaim(std::shared_ptr<TagPointClaim> c)
{
    for (auto it = this->claims.begin(); it != this->claims.end(); it++)
    {
        if (c.get() == it->get())
        {
            this->claims.erase(it);
            break;
        }
    }

    this->rerender();
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
    // Clean finished items.
    // Otherwise we will be overwritten by something that shouldn't even be there.
    this->cleanFinished();

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

/// Set all vals to 0 if there are no claims
/// Do not trigger subscribers
/// Call from within a "bang" type one shot handler
/// If you want to make it so setting it to 1 again triggers it again.

void TagPoint::silentResetValue()
{
    for (int i = 0; i < this->count; i++)
    {
        this->background_value[i] = 0;

        if (this->claims.size() > 0)
        {
            this->value[i] = 0;
        }
    }
}

void TagPoint::addClaim(std::shared_ptr<TagPointClaim> claim)
{
    // This check should usually be fast since it's rare to have more than a few claims
    for (auto it = this->claims.begin(); it != this->claims.end(); it++)
    {
        if (claim.get() == it->get())
        {
            return;
        }
    }
    this->claims.push_back(claim);
    std::sort(this->claims.begin(), this->claims.end(), claimCmpGreater);
};

void TagPoint::rerender()
{

    // If there are no claims, we don't need to do anything except
    // Check changes and notify subscribers
    if (this->claims.size() == 0)
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
        return;
    }

    // This is used for change detection.
    TAG_DATA_TYPE buffer[sizeof(TAG_DATA_TYPE) * this->count];
    memcpy(buffer, this->value, sizeof(TAG_DATA_TYPE) * this->count); // flawfinder: ignore

    memcpy(this->value, this->background_value, sizeof(TAG_DATA_TYPE) * this->count); // flawfinder: ignore

    bool unfinished_passed = false;

    // How many finished ones are at the start.
    // We can delete these after we are done with them.
    int finished_count = 0;

    for (const auto &claim : this->claims)
    {
        claim->applyLayer(this->value, this->count);

        if (!unfinished_passed)
        {
            if (claim->finished)
            {
                memcpy(this->background_value + claim->startIndex,
                       this->value + claim->startIndex,
                       sizeof(TAG_DATA_TYPE) * claim->count);
                finished_count += 1;
            }
        }
        else
        {
            unfinished_passed = true;
        }
    }

    // Delete finished claims
    while (finished_count)
    {
        this->claims.erase(this->claims.begin());
        finished_count--;
    }
    this->floatFirstValueCache = ((float)this->value[0]) * this->scale_inverse;

    if (memcmp(buffer, this->value, sizeof(TAG_DATA_TYPE) * this->count) != 0)
    {
        // Only first value changes affect ws api
        if (this->value[0] != buffer[0])
        {
            this->webAPIDirty = true;
        }

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

void TagPointClaim::applyLayer(TAG_DATA_TYPE *old, uint16_t count)
{
    for (int i = 0; i < this->count; i++)
    {
        // Don't go past end
        if ((this->startIndex + i) >= count)
        {
            break;
        }
        old[this->startIndex + i] = this->value[this->startIndex + i];
    }
}
