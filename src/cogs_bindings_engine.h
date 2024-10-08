
/**
 * @file
 * This lets you define rules that set the value of a tag point in terms of an expression.
 */

#pragma once

#include <tr1/unordered_map>
#include <string>
#include <map>
#include <vector>
#include <cstring>

#include <memory>
#include <stdint.h>
#include "tagpoint_template.h"
#include <Arduino.h>
extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

namespace cogs_rules
{
  typedef cogs_tagpoints::TagPoint IntTagPoint;

  /// Constants available in the expressions
  extern std::map<std::string, float *> constants;

  /// 0 argument functions to make available to users
  extern std::map<std::string, float (*)()> user_functions0;
  
  /// 1 argument functions to make available to users
  extern std::map<std::string, float (*)(float)> user_functions1;
  
  /// 2 argument functions to make available to users
  extern std::map<std::string, float (*)(float)> user_functions2;

  /// compile an expression, that will have access to cogs vars.
  te_expr * compileExpression(const std::string &input);

  class Binding;
  class Clockwork;

  //! Must be called after any changes to IntTagPoints, constants, or user functions
  //! Before anything is eval'ed
  //! Will not alter state of any variables that already exist.
  void refreshBindingsEngine();

  /// This number is used throughout the code whenever we need to represent a fraction
  /// As an integer, it is the default fixed point resolution.
  /// This is special because it is the highest power of two fitting in an int16.
  const int32_t FXP_RES = 16384;

  class IntFadeClaim : public cogs_tagpoints::TagPointClaim
  {
  public:
    int32_t start;
    int32_t duration;
    int32_t alpha;
    IntFadeClaim(unsigned long start, 
    unsigned long duration, 
    int32_t * values,
    int offset = 0,
    int count = 1,
    uint16_t alpha = 16384);

    virtual void applyLayer(int32_t * vals, int count) override;
  };

  class Binding
  {
  private:
    te_expr *input_expression;
    std::shared_ptr<IntTagPoint> target;
    std::string target_name;

    // In claim mode we use a claim rather than directly setting the value
    std::shared_ptr<cogs_tagpoints::TagPointClaim> claim = nullptr;

    /// Don't run till unfrozen
    bool frozen = false;
  public:

    // A binding can be for an array. This is an index and count for what part
    // Of the tag point's data to affect.
    unsigned int multiStart =0;
    unsigned int multiCount =1;

    float fadeInTime = 0.0;


    /// If True, act once when entering a state, even if no changes
    bool onenter = false;

    /// If True, only act when value changes, and on enter if
    /// onenter is set.
    bool onchange = false;


    /// Only act once on enter no matter what.
    bool freeze = false;

    /// Array tracking the last value of the binding.
    /// Used for change detection.
    int * lastState = nullptr;

    Binding(const std::string &target_name, const std::string & input);

    //! Eval the expression, do change detection.
    //! If the value changes, notify target
    void eval();

    //! Reset the change detection and set up when the binding becomes active
    void enter();

    //! Call when the binding becomes inactive
    void exit();



    ~Binding();
  };

  //! Represents one state machine state.
  //! Does not have a name because Clockwork tracks that
  class State
  {
  private:
    friend class Clockwork;
    // The tag that tells us if we're in this state
    std::shared_ptr<cogs_tagpoints::IntTagPoint> tag = nullptr;

  public:
    /// Duration in milliseconds. If set,
    /// The state will transition to the next state after this many milliseconds.
    int32_t duration = 0;

    /// The name of the next state to transition to when the state is finished.
    std::string nextState = "";

    std::shared_ptr<Clockwork> owner;

    std::string name;
    //! List of bindings which shall be evaluated only when the state is active
    std::vector<std::shared_ptr<cogs_rules::Binding>> bindings;

    //! Reviewuate all bindings in this state.
    void eval();

    //! Called when the state is exited or the binding otherwise becomes inactive
    void exit();

    void enter();

    //! Don't create bindings yourself, use this
    std::shared_ptr<cogs_rules::Binding> addBinding(std::string target_name, std::string input);

    void clearBindings();
    void removeBinding(std::shared_ptr<cogs_rules::Binding> binding);

    ~State();
  };

  //! A clockwork is a state machine with extra features.
  class Clockwork
  {
  private:
  public:
    /// Do not directly create a Clockwork.  Use Clockwork::getClockwork
    explicit Clockwork(const std::string &name);

    std::string name;
    State * currentState = nullptr;
    unsigned long enteredAt;

    //! Eval all clockworks
    static void evalAll();

    inline static std::map<std::string, std::shared_ptr<cogs_rules::Clockwork>> allClockworks;

    //! Get a clockwork or make one of one by that name doesn't exist
    /// This is the only way to get a clockwork.
    static std::shared_ptr<Clockwork> getClockwork(std::string);

    static inline bool exists(const std::string &name){
      if (allClockworks.find(name) == allClockworks.end())
      {
        return false;
      }
      else{
        return true;
      }
    }

    //! Unregister this clockwork from the global list.
    //! The object should not be used after this call.
    void close();

    //! A clockwork is a state machine with extra features.
    std::map<std::string, std::shared_ptr<cogs_rules::State>> states;

    /// Create a state object or get it if it already exists
    std::shared_ptr<cogs_rules::State> getState(std::string);

    /// Immediately transition to a new state
    void gotoState(const std::string &, unsigned long time = 0);

    /// Delete a state
    void removeState(std::string);

    void eval();

    ~Clockwork();

  private:
    /// Clockworks can own tag points. They are auto unregistered.
    std::vector<std::shared_ptr<IntTagPoint>> tags;
  };

  std::shared_ptr<cogs_rules::Binding> makeBinding(std::string target_name, std::string input);
  std::shared_ptr<cogs_rules::Clockwork> makeClockwork(std::string name);

  void  setupRulesEngine();
}