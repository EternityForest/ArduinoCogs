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
  class TagPointClaim;


  /// A tag point is a container representing a subscribable value.
  /// They may be of any type, however int32 is the standard type
  /// Tags must be unregistered once you are done, as they automatically store themselves in a global list.

  /// Do not directly create a TagPoint.  Use TagPoint::getTag(name, default_value)

  class TagPoint
  {
  private:


    // Track all claims affecting the tag's value
    // we can only have one claim of each priority.
    // Unsigned long is so we can use more complex keys.
    std::vector<std::shared_ptr<TagPointClaim>> claims;

    // These functions are called when value changes
    std::vector<void (*)(TagPoint *)> subscribers;


  public:
    TagPoint(const std::string &n, TAG_DATA_TYPE val, int count = 1);
    ~TagPoint();

    /// This is the current "base" value before applying any claims.
    /// It may be overridden or filtered by a claim.  Once a claim is "finished",
    /// Meaning it's effect is no longer changing, the output of that layer will be written to
    TAG_DATA_TYPE *background_value;

    /// The last rendered first value, converted to a float.
    float floatFirstValueCache;

    /// Tag points can represent multiple values.  Normally a tag only has one value
    /// but some are arrays
    uint16_t count = 1;

    /// Set fixed point resolution, 1 in float user expressions
    /// is this many when converted to int.
    void setScale(int s)
    {
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
    @param default_value The default value of the tag.  Ignored if tag already exists.
    */

    static std::shared_ptr<TagPoint> getTag(std::string name, TAG_DATA_TYPE default_value, int count = 1)
    {
      bool is_new = !TagPoint::exists(name);

      if (name.size() == 0)
      {
        throw std::runtime_error("name empty");
      }

      if (count == 0)
      {
        throw std::runtime_error("0-length tag not allowed");
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

    /// Recalculates value, applying all claim layers
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

    //! Create an override claim

    /*!
    @param layer Serves as both unique ID and priority for the claim
    @param value the value of the claim
    */

    std::shared_ptr<TagPointClaim> overrideClaim(int layer, TAG_DATA_TYPE value, int startIndex = 0, int count = 1);

    //! Remove a claim.  There is no way to add it back.
    void removeClaim(std::shared_ptr<TagPointClaim> c);

    //! Function will be called when val changes
    void subscribe(void (*func)(TagPoint *));

    //! Unsubscribe a previously subscribed function if it exists.
    void unsubscribe(void (*func)(TagPoint *));

    //! Set the background value of a tag.
    /// Does not automatically rerender.
    /// If count is 0, sets all values(So we can pretend it's a scalar)
    /// If count is not 0, sets only the first count values after startIndex.
    void setValue(TAG_DATA_TYPE val, int startIndex = 0, int count = 0);

    void addClaim(std::shared_ptr<TagPointClaim> claim);

    void silentResetValue();

    /// Clean the list of claims.  Once something is marked finished, if it is
    /// Right above background, it's value is the new background and we delete it
    void cleanFinished();
  };

  /// A tag point claim is an an object that sets the value of a tag.
  /// It is essentially a layer and can even blend with layers below.
  /// A claim has no idea about it's parent tag and changes do not affect it unless you
  /// Explicitly rerender.

  class TagPointClaim
  {

  public:
    /// Every claim has an array of values
    /// that it wants to put at a specific place.
    /// Normally we just have one val at 0 to represent scalars
    TAG_DATA_TYPE *value;

    /// Index into the tagpoint's value array where we are affecting
    int startIndex = 0;

    /// How many vals
    int count = 1;

    int priority = 0;
    bool finished = false;

    // applyLayer is an in place paint operation
    virtual void applyLayer(TAG_DATA_TYPE *old, int count);

    /// Count must be specified up front, everything else can be done later
    /// Because it's mostly type specific
    TagPointClaim(int startIndex, int count);

    ~TagPointClaim();
  };


}