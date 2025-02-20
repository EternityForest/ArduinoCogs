#pragma once
#include <string>
#include <map>
#include <vector>

#include <memory>
#include <stdint.h>
#include <cstring>
#include "cogs_util.h"
#include "cogs_global_events.h"

extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

#define TAG_DATA_TYPE int32_t

namespace cogs_tagpoints
{
  /// A tag point is a container representing a subscribable value.
  /// They may be of any type, however int32 is the standard type
  /// Tags must be unregistered once you are done, as they automatically store themselves in a global list.

  /// Do not directly create a TagPoint.  Use TagPoint::getTag(name, default_value)

  class TagPoint
  {
  private:
    // These functions are called when value changes
    std::vector<void (*)(TagPoint *)> subscribers;

    void notifySubscribers();

  public:
    TagPoint(const std::string &n, TAG_DATA_TYPE val, uint16_t count = 1);
    ~TagPoint();

    TAG_DATA_TYPE *background_value;

    /// The last rendered first value, converted to a float.
    float floatFirstValueCache;

    /// Tag points can represent multiple values.  Normally a tag only has one value
    /// but some are arrays
    uint16_t count = 1;


    /// Can users write to the tag or just read?
    bool readOnly = false;

    // Used for the web ui to mark if it needs to push data.
    // we set it to true when the first value in the array changes,
    // not on any other val.
    bool webAPIDirty = false;

    /// The time of the previous push to subscribers, set AFTER all subscribers have been notified.
    /// Useful for rate limiting logic
    unsigned long lastChangeTime = 0;

    /// Set fixed point resolution, 1 in float user expressions
    /// is this many when converted to int.
    void setScale(int s)
    {
      if(s==0)
      {
        cogs::logError("Tag scale cannot be 0 in "+this->name);
      }
      this->scale = s;
      this->scale_inverse = 1.0f / scale;
    }

    /// The scale is a multiplier used to convert floats to the actual tag value.
    /// Do not set directly.  Use setScale
    int scale = 1;

    /// Used to convert tag values to floats
    /// Do not set directly.  Use setScale
    float scale_inverse = 1;

    /// The range of values.  Used for metadata only, for performance reasons
    /// We do not constrain automatically in most cases
    int min = INT32_MIN;
    /// The range of values.  Used for metadata only, for performance reasons
    /// We do not constrain automatically in most cases
    int max = INT32_MAX;

    /// The min value considered normal
    int lo = INT32_MIN;

    /// The max value considered normal. Used for web display.
    int hi = INT32_MAX;

    /// The unit of the tag. Used for display purposes mostly.
    std::shared_ptr<const std::string> unit = cogs::getSharedString("");
    std::shared_ptr<const std::string> description = cogs::getSharedString("");

    /// Map of all tags of the given type
    inline static std::vector<std::shared_ptr<TagPoint>> all_tags;

    /// Lets the calling code add some arbitrary data
    void *extraData = nullptr;

    std::string name;

    /// The current values after having rendered.
    TAG_DATA_TYPE *value;

    //! Get a tag by name and create it if it doesn't exist.
    //! Do not create a tag directly, use TagPoint::getTag

    /*!
    @param name The name of the tag
    @param default_value How many values the tag has
    @param default_value The default value of the tag.  Ignored if tag already exists.
    @param default_scale The default scale of the tag.  Ignored if tag already exists.

    */

    static std::shared_ptr<TagPoint> getTag(std::string name, 
    TAG_DATA_TYPE default_value, int count = 1,
    int default_scale = 1)
    {
      bool is_new = !TagPoint::exists(name);

      if (name.size() == 0)
      {
        cogs::logError("Tag name cannot be empty");
        return nullptr;
      }

      if (count == 0)
      {
        cogs::logError("Tag count cannot be 0");
        return nullptr;
      }

      if (!TagPoint::exists(name))
      {
        TagPoint::all_tags.push_back(std::shared_ptr<TagPoint>(new TagPoint(name, default_value, count)));
      }

      if (is_new)
      {
        cogs::triggerGlobalEvent(cogs::tagCreatedEvent, 0, name);
      }

      for (auto const &tag : TagPoint::all_tags)
      {
        if (tag->name == name)
        {
          if(is_new)
          {
            tag->setScale(default_scale);
          }
          return tag;
        }
      }

      throw std::runtime_error("??");
      return nullptr;
    }

    //! Check if a tag exists
    static bool exists(const std::string &name)
    {
      for (auto const &tag : TagPoint::all_tags)
      {
        if (tag->name == name)
        {
          return true;
        }
      }
      return false;
    }

    //! Unregister a tag. It should not be used after that.
    void unregister();

    /// Recalculates value
    void rerender();

    //! Set the description of the tag.
    void setDescription(const std::string &desc)
    {
      description = cogs::getSharedString(desc);
    }

    //! Set the unit of the tag. Uses string interning for efficiency
    void setUnit(const std::string &u)
    {
      this->unit = cogs::getSharedString(std::string(u));
    }


    //! Function will be called when val changes
    void subscribe(void (*func)(TagPoint *));

    //! Unsubscribe a previously subscribed function if it exists.
    void unsubscribe(void (*func)(TagPoint *));

    //! Set the background value of a tag.
    /// Does not automatically rerender.
    /// If count is 0, sets all values(So we can pretend it's a scalar)
    /// If count is not 0, sets only the first count values after startIndex.
    void setValue(TAG_DATA_TYPE val, uint16_t startIndex = 0, uint16_t count = 0);

    /// Set the value only if the first val is more than minDifference
    /// from the last val, or if it's been more than interval ms.
    void smartSetValue(TAG_DATA_TYPE val, int minDifference, int interval);


    void silentResetValue();

    /// Increment the value by 1, looping at 1M, and skipping 0.
    void bang(){
      this->setValue(cogs::bang(this->value[0]));
    }
    
  };

}