
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
#include <freertos/FreeRTOS.h>

extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

#define CLAIM_PRIORITY_FADE 2

namespace cogs_rules
{

  extern bool needRefresh;
  typedef cogs_tagpoints::TagPoint IntTagPoint;

  /// Constants available in the expressions
  extern std::map<std::string, float *> constants;

  /// 0 argument functions to make available to users
  extern std::map<std::string, float (*)()> user_functions0;

  /// 1 argument functions to make available to users
  extern std::map<std::string, float (*)(float)> user_functions1;

  /// 2 argument functions to make available to users
  extern std::map<std::string, float (*)(float, float)> user_functions2;

  extern std::map<std::string, float (*)(float, float, float)> user_functions3;

  /// compile an expression, that will have access to cogs vars.
  te_expr *compileExpression(const std::string &input);

  /// evaluate an expression and return the result
  float evalExpression(const std::string &input);

  class Binding;
  class Clockwork;

  //! Must be called after any changes to IntTagPoints, constants, or user functions
  //! Before anything is eval'ed, and before you unlock and leave the context.
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
    uint16_t alpha;
    bool fadeDone = false;
    virtual void applyLayer(int32_t *vals, uint16_t start) override;

    IntFadeClaim(uint16_t startIndex, uint16_t count);
  };

  class Binding
  {
  private:
    std::shared_ptr<IntTagPoint> target;
    std::string target_name;

    // In claim mode we use a claim rather than directly setting the value
    std::shared_ptr<cogs_rules::IntFadeClaim> claim = nullptr;

    /// Don't run till unfrozen
    bool frozen = false;

  public:
    std::string inputExpressionSource;

    te_expr *inputExpression;

    // A binding can be for an array. This is an index and count for what part
    // Of the tag point's data to affect.
    uint16_t multiStart = 0;
    uint16_t multiCount = 1;

    /// Should we apply to bg value, or create a claim with priority?
    uint16_t layer = 0;

    // Eval'ed when the binding enters
    std::string fadeInTimeSource;

    te_expr *fadeInTime = nullptr;

    // Eval'ed every frame
    std::string alphaSource;
    te_expr *alpha = nullptr;

    /// If you change fadeInTime, set up the target again
    bool trySetupTarget();

    /// If True, act once when entering a state, even if no changes
    bool onenter = false;

    /// If True, only act when value changes, and on enter if
    /// onenter is set.
    bool onchange = false;

    /// Only act once on enter no matter what.
    bool freeze = false;

    /// Array tracking the last value of the binding.
    /// Used for change detection.
    int *lastState = nullptr;

    Binding(const std::string &target_name, const std::string &input);

    //! Eval the expression, do change detection.
    //! If the value changes, notify target
    void eval();

    //! Reset the change detection and set up when the binding becomes active
    void enter();

    //! Call when the binding becomes inactive
    void exit();

    ~Binding();
    bool handleEngineRefresh();
  };

  //! Represents one state machine state.
  //! Does not have a name because Clockwork tracks that
  class State
  {
  private:
    friend class Clockwork;
    // The tag that tells us if we're in this state
    std::shared_ptr<cogs_rules::IntTagPoint> tag = nullptr;

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
    bool handleEngineRefresh();
  };

  //! A clockwork is a state machine with extra features.
  class Clockwork
  {
  private:
  public:
    /// Do not directly create a Clockwork.  Use Clockwork::getClockwork
    explicit Clockwork(const std::string &name);

    std::string name;
    State *currentState = nullptr;
    unsigned long enteredAt;

    //! Eval all clockworks
    static void evalAll();

    inline static std::map<std::string, std::shared_ptr<cogs_rules::Clockwork>> allClockworks;

    //! Get a clockwork or make one of one by that name doesn't exist
    /// This is the only way to get a clockwork.
    static std::shared_ptr<Clockwork> getClockwork(std::string);

    static inline bool exists(const std::string &name)
    {
      if (allClockworks.find(name) == allClockworks.end())
      {
        return false;
      }
      else
      {
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
    bool handleEngineRefresh();

  private:
    /// Clockworks can own tag points. They are auto unregistered.
    std::vector<std::shared_ptr<IntTagPoint>> tags;
  };

  std::shared_ptr<cogs_rules::Binding> makeBinding(std::string target_name, std::string input);
  std::shared_ptr<cogs_rules::Clockwork> makeClockwork(std::string name);

  void begin();
}