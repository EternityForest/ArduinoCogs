
/**
 * @file
 * This lets you define rules that set the value of a tag point in terms of an expression.
 */

#pragma once

#include <tr1/unordered_map>
#include <string>
#include <map>
#include <list>
#include <cstring>

#include <memory>
#include <stdint.h>
#include "tagpoint_template.h"
#include <Arduino.h>
extern "C"
{
#include "tinyexpr.h"
}

namespace cogs_rules
{
  typedef cogs_tagpoints::TagPoint<int32_t> IntTagPoint;

  class Binding;
  class Clockwork;

  //! Must be called after any changes to IntTagPoints
  //! Before anything is eval'ed
  //! Will not alter state of any variables that already exist.
  void refresh_bindings_engine();

  /// This number is used throughout the code whenever we need to represent a fraction
  /// As an integer, it is the default fixed point resolution.
  /// This is special because it is the highest power of two fitting in an int16.
  const int32_t FXP_RES = 16384;

  class IntFadeClaim : public cogs_tagpoints::TagPointClaim<int32_t>
  {
  public:
    int32_t start;
    int32_t duration;
    int32_t alpha;
    IntFadeClaim(unsigned long start, unsigned long duration, int32_t value, uint16_t alpha);

    int32_t applyLayer(int32_t bg);
  };

  class Binding
  {
  private:
    te_expr *input_expression;
    std::shared_ptr<IntTagPoint> target;
    float last_value = -NAN;

  public:
    Binding(std::string target_name, std::string input);

    //! Eval the expression, do change detection.
    //! If the value changes, notify target
    void eval();

    //! Reset the change detection.
    void reset();

    ~Binding();
  };

  //! Represents one state machine state.
  //! Does not have a name because Clockwork tracks that
  class State
  {
  private:
    friend class Clockwork;


  public:

    /// Duration in milliseconds. If set,
    /// The state will transition to the next state after this many milliseconds.
    int32_t duration=0;

    /// The name of the next state to transition to when the state is finished.
    std::string nextState = "";

    std::shared_ptr<Clockwork> owner;

    std::string name;
    //! List of bindings which shall be evaluated only when the state is active
    std::list<std::shared_ptr<cogs_rules::Binding>> bindings;

    //! Reviewuate all bindings in this state.
    void eval();

    //! Reset the change detection of all bindings.
    void reset();

    //! Don't create bindings yourself, use this
    std::shared_ptr<cogs_rules::Binding> addBinding(std::string target_name, std::string input);

    void clearBindings();
    void removeBinding(std::shared_ptr<cogs_rules::Binding> binding);
  };

  //! A clockwork is a state machine with extra features.
  class Clockwork
  {
  private:

  public:

    /// Do not directly create a Clockwork.  Use Clockwork::getClockwork
    Clockwork(std::string name);

    std::string name;
    std::string currentState;
    unsigned long enteredAt;

    //! Eval all clockworks
    static void evalAll();

    static std::map<std::string, std::shared_ptr<cogs_rules::Clockwork>> allClockworks;

    //! Get a clockwork or make one of one by that name doesn't exist
    /// This is the only way to get a clockwork.
     static std::shared_ptr<Clockwork> getClockwork(std::string);

    //! Unregister this clockwork from the global list.
    //! The object should not be used after this call.
    void close();

    //! A clockwork is a state machine with extra features.
    std::map<std::string, std::shared_ptr<cogs_rules::State>> states;


    /// Create a state object or delete it if it already exists
    std::shared_ptr<cogs_rules::State> getState(std::string);

    /// Immediately transition to a new state
    void gotoState(std::string);

    /// Delete a state
    void removeState(std::string);

    void eval();

    ~Clockwork();

  private:
    /// Clockworks can own tag points. They are auto unregistered.
    std::list<std::shared_ptr<IntTagPoint>> tags;
  };

  std::shared_ptr<cogs_rules::Binding> makeBinding(std::string target_name, std::string input);
  std::shared_ptr<cogs_rules::Clockwork> makeClockwork(std::string name);

}