#pragma once
#include <tr1/unordered_map>
#include <string>
#include <map>
#include <list>

#include <memory>
#include <stdint.h>

extern "C"
{
#include "tinyexpr.h"
}

namespace cogs_tagpoints
{

  template <typename T>
  class TagPointClaim;

  /// A tag point is a container representing a subscribable value.
  /// They may be of any type, however int32 is the standard type

  template <typename T>
  class TagPoint
  {
  private:
    // This is the current "base" value before applying any claims.
    // It may be overridden or filtered by a claim.  Once a claim is "finished",
    // Meaning it's effect is no longer changing, the output of that layer will be written to
    T background_value;

    // Track all claims affecting the tag's value
    // we can only have one claim of each priority.
    std::map<int, std::shared_ptr<TagPointClaim<T>>> claims;

    // These functions are called when value changes
    std::list<void (*)(T)> subscribers;

    /// Clean the list of claims.  Once something is marked finished, if it is
    /// Right above background, it's value is the new background and we delete it
    void clean_finished();

  public:
    /// Map of all tags of the given type
    static std::map<std::string, std::shared_ptr<TagPoint>> all_tags;

    std::string name;
    T last_calculated_value;
    TagPoint(std::string n, T val);

    /// Recalculates value, applying all claim layers
    T rerender();

    //! Create an override claim

    /*!
    @param layer Serves as both unique ID and priority for the claim
    @param value the value of the claim
    */

    std::shared_ptr<TagPointClaim<T>> overrideClaim(int layer, T value);

    //! Remove a claim.  There is no way to add it back.
    void removeClaim(std::shared_ptr<TagPointClaim<T>> c);

    //! Function will be called when val changes
    void subscribe(void (*func)(T));

    //! Unsubscribe a previously subscribed function if it exists.
    void unsubscribe(void (*func)(T));

    //! Set the background value of a tag.
    /// Does not automatically rerender.

    void setValue(T val);

    void addClaim(std::shared_ptr<TagPointClaim<T>> claim);
  };

  /// A tag point claim is an an object that sets the value of a tag.
  /// It is essentially a layer and can even blend with layers below.
  /// A claim has no idea about it's parent tag and changes do not affect it unless you
  /// Explicitly rerender.
  template <typename T>
  class TagPointClaim
  {

  public:
    T value;
    int priority;
    bool finished;
    T applyLayer(T);
  };

  template <typename T>
  class GetterClaim : TagPointClaim<T>
  {
  public:
    T(*getter)
    ();

    GetterClaim(T (*getter_func)());
  };

  // IMPLEMENTATION
  template <typename T>
  T TagPointClaim<T>::applyLayer(T)
  {
    // If this were true we could just fold the val into background and get rid of it.
    this->finished = false;
    return this->value;
  };

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
  TagPoint<T>::TagPoint(std::string n, T val)
  {
    this->name = n;
    this->background_value = val;
    this->last_calculated_value = val;

    std::shared_ptr<TagPoint<T>> p(this);
    TagPoint<T>::all_tags[n] = p;
  }

  template <typename T>
  std::shared_ptr<TagPointClaim<T>> TagPoint<T>::overrideClaim(int layer, T value)
  {
    TagPointClaim<T> *c = new TagPointClaim<T>();
    std::shared_ptr<TagPointClaim<T>> p(c);

    c->value = value;
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
  void TagPoint<T>::subscribe(void (*func)(T))
  {
    // Defensive null crash blocking
    if (func)
    {
      // Ensure idempotency
      this->subscribers.remove(func);
      this->subscribers.insert(0, func);
    }
  };

  template <typename T>
  void TagPoint<T>::unsubscribe(void (*func)(T))
  {
    this->subscribers.remove(func);
  };

  template <typename T>
  void TagPoint<T>::setValue(T val)
  {
    // Clean finished items.
    // Otherwise we will be overwritten by something that shouldn't even be there.
    this->clean_finished();
    this->background_value = val;
    this->rerender();
  };

  template <typename T>
  void TagPoint<T>::addClaim(std::shared_ptr<TagPointClaim<T>> claim)
  {
    this->claims[claim->priority] = claim;
  };

  template <typename T>
  T TagPoint<T>::rerender()
  {
    T v = this->background_value;

    bool unfinished_passed = false;

    // How many finished ones are at the start.
    // We can delete these after we are done with them.
    int finished_count = 0;

    for (const auto &[key, claim] : this->claims)
    {
      v = claim->applyLayer(v);

      if (!unfinished_passed)
      {
        if (claim->finished)
        {
          this->background_value = v;
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
    this->last_calculated_value = v;
    return v;
  };
}