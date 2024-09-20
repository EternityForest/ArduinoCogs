#pragma once
#include <tr1/unordered_map>
#include <string>
#include <map>
#include <list>

#include <memory>
#include <stdint.h>
#include <cstring>
#include "cogs_util.h"

extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

namespace cogs_tagpoints
{

  template <typename T>
  class TagPointClaim;

  /// A tag point is a container representing a subscribable value.
  /// They may be of any type, however int32 is the standard type
  /// Tags must be unregistered once you are done, as they automatically store themselves in a global list.

  /// Do not directly create a TagPoint.  Use TagPoint<T>::getTag(name, default_value)

  template <typename T>
  class TagPoint
  {
  private:
    // This is the current "base" value before applying any claims.
    // It may be overridden or filtered by a claim.  Once a claim is "finished",
    // Meaning it's effect is no longer changing, the output of that layer will be written to
    T *background_value;

    // Track all claims affecting the tag's value
    // we can only have one claim of each priority.
    // Unsigned long is so we can use more complex keys.
    std::map<unsigned long, std::shared_ptr<TagPointClaim<T>>> claims;

    // These functions are called when value changes
    std::list<void (*)(TagPoint<T> *)> subscribers;

    /// Clean the list of claims.  Once something is marked finished, if it is
    /// Right above background, it's value is the new background and we delete it
    void clean_finished();

  public:
    TagPoint(std::string n, T val, int count = 1);
    ~TagPoint();


    /// Tag points can represent multiple values.  Normally a tag only has one value
    /// but some are arrays
    int count = 1;


    /// Map of all tags of the given type
    inline static std::map<std::string, std::shared_ptr<TagPoint>> all_tags;

    /// Lets the calling code add some arbitrary data
    void *extraData;

    std::string name;

    /// The current values after having rendered.
    T *value;

    //! Get a tag by name and create it if it doesn't exist.
    //! Do not create a tag directly, use TagPoint<T>::getTag

    /*!
    @param name The name of the tag
    @param default_value The default value of the tag.  Ignored if tag already exists.
    */

    inline static std::shared_ptr<TagPoint<T>> getTag(std::string name, T default_value, int count = 1)
    {
      if(name.size() == 0){
        throw std::runtime_error("name empty");
      }

      if(count == 0){
        throw std::runtime_error("0-length tag not allowed");
      }
      
      if (!TagPoint<T>::all_tags.contains(name))
      {
        TagPoint<T>::all_tags[name] = std::shared_ptr<TagPoint<T>>(new TagPoint<T>(name, default_value, count));
      }

      return TagPoint<T>::all_tags[name];
    }

    inline static bool exists(std::string name)
    {
      return TagPoint<T>::all_tags.contains(name);
    }
    //! Unregister a tag. It should not be used after that.
    void unregister()
    {
      TagPoint<T>::all_tags.erase(name);
    }

    /// Recalculates value, applying all claim layers
    void rerender();

    //! Create an override claim

    /*!
    @param layer Serves as both unique ID and priority for the claim
    @param value the value of the claim
    */

    std::shared_ptr<TagPointClaim<T>> overrideClaim(int layer, T value, int startIndex = 0, int count = 1);

    //! Remove a claim.  There is no way to add it back.
    void removeClaim(std::shared_ptr<TagPointClaim<T>> c);

    //! Function will be called when val changes
    void subscribe(void (*func)(TagPoint<T> *));

    //! Unsubscribe a previously subscribed function if it exists.
    void unsubscribe(void (*func)(TagPoint<T> *));

    //! Set the background value of a tag.
    /// Does not automatically rerender.
    /// If count is 0, sets all values(So we can pretend it's a scalar)
    /// If count is not 0, sets only the first count values after startIndex.
    void setValue(T val, int startIndex = 0, int count = 0);

    void addClaim(std::shared_ptr<TagPointClaim<T>> claim);
  };

  template <typename T>
  TagPoint<T>::TagPoint(std::string n, T val, int count)
  {
    this->name = n;
    // Dynamic allocation because we don't know how big this will be.
    this->background_value = (T *)malloc(sizeof(T) * count);
    this->value = (T *)malloc(sizeof(T) * count);

    // Set the value to be all the same
    for (int i = 0; i < count; i++)
    {
      this->background_value[i] = val;
    }
    this->rerender();
  }

  template <typename T>
  TagPoint<T>::~TagPoint()
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
  template <typename T>
  void TagPoint<T>::clean_finished()
  {
    bool unfinished_passed = false;

    // How many finished ones are at the start.
    // We can delete these after we are done with them.
    int finished_count = 0;

    for (const auto &[key, claim] : this->claims)
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
      this->claims.erase(0);
      finished_count--;
    }
  };

  template <typename T>
  std::shared_ptr<TagPointClaim<T>> TagPoint<T>::overrideClaim(int layer, T value, int startIndex, int count)
  {
    TagPointClaim<T> *c = new TagPointClaim<T>(count);
    c->startIndex = startIndex;
    std::shared_ptr<TagPointClaim<T>> p(c);

    for (int i = 0; i < count; i++)
    {
      c->value[i] = value;
    }

    c->priority = layer;

    this->claims[layer] = p;

    return p;
  }

  template <typename T>
  void TagPoint<T>::removeClaim(std::shared_ptr<TagPointClaim<T>> c)
  {
    if (this->claims[c->priority] == c)
    {
      this->claims.erase(c->priority);
    }
    this->rerender();
  }

  template <typename T>
  void TagPoint<T>::subscribe(void (*func)(TagPoint<T> *))
  {
    // Defensive null crash blocking
    if (func)
    {
      // Ensure idempotency
      this->subscribers.remove(func);
      this->subscribers.push_back(func);
    }
  };

  template <typename T>
  void TagPoint<T>::unsubscribe(void (*func)(TagPoint<T> *))
  {
    this->subscribers.remove(func);
  };

  template <typename T>
  void TagPoint<T>::setValue(T val, int startIndex, int count)
  {

    if (count == 0)
    {
      count = this->count;
    }
    // Clean finished items.
    // Otherwise we will be overwritten by something that shouldn't even be there.
    this->clean_finished();

    int upTo = startIndex + count;

    for (int i = startIndex; i < upTo; i++)
    {
      this->background_value[i] = val;
    }

    // Render makes the background values real if needed and notifies subscribers
    // If the rendered value changes.
    this->rerender();
  };

  template <typename T>
  void TagPoint<T>::addClaim(std::shared_ptr<TagPointClaim<T>> claim)
  {
    this->claims[claim->priority] = claim;
  };

  template <typename T>
  void TagPoint<T>::rerender()
  {

    // If there are no claims, we don't need to do anything except
    // Check changes and notify subscribers
    if (this->claims.size() == 0)
    {
      if (memcmp(this->value, this->background_value, sizeof(T) * this->count) != 0)
      {
        memcpy(this->value, this->background_value, sizeof(T) * this->count); // flawfinder: ignore
        for (const auto subscriber : this->subscribers)
        {
          subscriber(this);
        }
      }
      return;
    }

    // This is used for change detection.
    T buffer[sizeof(T) * this->count];
    memcpy(buffer, this->value, sizeof(T) * this->count); // flawfinder: ignore

    memcpy(this->value, this->background_value, sizeof(T) * this->count); // flawfinder: ignore

    bool unfinished_passed = false;

    // How many finished ones are at the start.
    // We can delete these after we are done with them.
    int finished_count = 0;

    for (const auto &[key, claim] : this->claims)
    {
      claim->applyLayer(this->value, this->count);

      if (!unfinished_passed)
      {
        if (claim->finished)
        {
          memcpy(this->background_value + claim->startIndex,
                 this->value + claim->startIndex,
                 sizeof(T) * claim->count);
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
      this->claims.erase(0);
      finished_count--;
    }

    // Push data to subscribers
    for (auto func : this->subscribers)
    {
      func(this);
    }
  };

  template class TagPoint<int32_t>;

  /// A tag point claim is an an object that sets the value of a tag.
  /// It is essentially a layer and can even blend with layers below.
  /// A claim has no idea about it's parent tag and changes do not affect it unless you
  /// Explicitly rerender.
  template <typename T>
  class TagPointClaim
  {

  public:
    /// Every claim has an array of values
    /// that it wants to put at a specific place.
    /// Normally we just have one val at 0 to represent scalars
    T *value;

    /// Index into the tagpoint's value array where we are affecting
    int startIndex = 0;

    /// How many vals
    int count = 1;

    int priority;
    bool finished;

    // applyLayer is an in place paint operation
    inline virtual void applyLayer(T *old, int count)
    {
      for (int i = 0; i < this->count; i++)
      {
        // Don't go past end
        if (i >= count)
        {
          break;
        }
        old[this->startIndex + i] = this->value[this->startIndex + i];
      }
    }

    /// Count must be specified up front, everything else can be done later
    /// Because it's mostly type specific
    TagPointClaim(int count)
    {
      this->finished = false;
      this->value = (T *)malloc(sizeof(T) * count);
      this->count = count;
    };

    ~TagPointClaim()
    {
      if (this->value != nullptr)
      {
        free(this->value);
      }
    };
  };

}
