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
  public:

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

  class Clockwork
  {
  public:
    Clockwork(std::string name);
    std::string name;
    std::string current_state;

    //! A clockwork is a state machine with extra features.
    std::map<std::string, std::shared_ptr<cogs_rules::State>> states;


    std::shared_ptr<cogs_rules::State> addState(std::string);
    void removeState(std::string);

    ~Clockwork();

  private:
      /// Clockworks can own tag points. They are auto unregistered.
      std::list<std::shared_ptr<IntTagPoint>> tags;
  };


  std::shared_ptr<cogs_rules::Binding> makeBinding(std::string target_name, std::string input);
  std::shared_ptr<cogs_rules::Clockwork> makeClockwork(std::string name);

  

}