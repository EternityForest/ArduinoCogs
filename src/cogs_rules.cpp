#include <Arduino.h>
#include <math.h>
#include "cogs_rules.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include <Regexp.h>
#include "reggshell/reggshell.h"
#include "cogs_reggshell.h"
#include "cogs_trouble_codes.h"

using namespace cogs_rules;

bool cogs_rules::needRefresh = false;
bool cogs_rules::started = false;

static std::vector<Binding> bindings;

/// This is a set of tinyexpr variables.
/// Note that we have modified tinyexpr so values are not floats,
/// We do this because we want to use fixed point internally and we do not know what vars will
/// Be requested until they actually are.
static int global_vars_count = 0;
static te_variable global_vars[256];

static Clockwork *ctx_cw = nullptr;

static float getStateAge()
{
  if (ctx_cw)
  {
    return (millis() - ctx_cw->enteredAt) / 1000.0;
  }
  return 0.0;
}

static void onStateTagSet(IntTagPoint *tag);
te_expr *cogs_rules::compileExpression(const std::string &input)
{
  int err = 0;

  // cogs::logInfo("Compiling: " + input);
  // // debug print global vars
  // cogs::logInfo("Global vars count: " + std::to_string(global_vars_count));
  // for (int i = 0; i < global_vars_count; i++){
  //   if(global_vars[i].name){
  //   cogs::logInfo(global_vars[i].name);
  //   }
  //   else{
  //     cogs::logInfo("NULL");
  //   }
  // }

  auto x = te_compile(input.c_str(), global_vars, global_vars_count, &err);
  if (err)
  {
    cogs::logError("Error compiling expression: " + input + " (" + std::to_string(err) + ")");
    return 0;
  }
  if (!x)
  {
    cogs::logError("Error compiling expression: " + input + " (unknown error)");
    return 0;
  }
  return x;
}

void cogs_rules::freeExpression(te_expr *x)
{
  te_free(x);
}

float cogs_rules::evalExpression(const std::string &input)
{
  auto x = compileExpression(input);
  if (!x)
  {
    return 0.0;
  }
  float f = te_eval(x);
  te_free(x);
  return f;
}

static void clear_globals()
{
  int i = 0;
  while (1)
  {
    if (global_vars[i].name)
    {
      // We put it there and know this isn't really const, it's dynamically malloced.
      free(const_cast<char *>(global_vars[i].name));
      global_vars[i].name = 0;
      global_vars[i].address = 0;
      global_vars[i].context = 0;
      global_vars[i].type = 0;
      i++;
    }
    else
    {
      break;
    }
  }
};

// Add a new global variable.
static void append_global(const std::string &name, const float *value)
{
  // Make a C compatible version of the string we need to manually free later
  char *n = static_cast<char *>(malloc(name.size() + 1));

  n[name.size()] = (char)NULL;
  memcpy(n, name.c_str(), name.size()); // flawfinder: ignore

  int i = 0;
  while (1)
  {
    if (global_vars[i].name == NULL)
    {
      global_vars[i].name = n;
      global_vars[i].address = value;
      global_vars[i].context = 0;
      global_vars[i].type = 0;
      break;
    }
    i++;
  }
};

// Add a new global variable.
static void append_func(const std::string &name, void *f, int type)
{
  // Make a C compatible version of the string we need to manually free later
  char *n = static_cast<char *>(malloc(name.size() + 1));

  n[name.size()] = (char)NULL;
  memcpy(n, name.c_str(), name.size()); // flawfinder: ignore

  int i = 0;
  while (1)
  {
    if (global_vars[i].name == NULL)
    {
      global_vars[i].name = n;
      global_vars[i].address = f;
      global_vars[i].context = 0;
      global_vars[i].type = type;
      break;
    }
    i++;
  }
};

static float fxp_res_var = FXP_RES;

static float dollar_sign_i = 0;

static float adcUserFunction(float x)
{
  return analogRead(x) / (4095 / 3.3);
}

static float uptimeFunction()
{
  return ((float)cogs::uptime()) / 1000.0f;
}
static float randomUserFunction()
{
  uint16_t r = cogs::random(); // flawfinder: ignore
  return (float)r / 65535.0f;
}

float clamp(float x, float min, float max)
{
  if (x < min)
  {
    return min;
  }
  if (x > max)
  {
    return max;
  }
  return x;
}

float min(float x, float y)
{
  if (x < y)
  {
    return x;
  }
  return y;
}

float max(float x, float y)
{
  if (x > y)
  {
    return x;
  }
  return y;
}

// Time in milliseconds, wrapping around every few hours
// Losing more than 10ms precision.

static float timeUserFunction()
{
  int time = millis() % 8388608;
  return (float)time;
}
static int flicker_flames[16];
static int flicker_flames_lp[16];
static unsigned long last_flicker_run[16];
static unsigned long wind_zones[4];
static unsigned long last_wind_run[4];

static float doFlicker(float idx)
{
  int index = idx;
  int wind_zone = (index / 1024) % 4;

  // Calculate simulated wind
  if (millis() - last_wind_run[wind_zone] > 20)
  {
    last_wind_run[wind_zone] = millis();

    if (cogs::random(0, 200) == 0) // flawfinder: ignore
    {
      wind_zones[wind_zone] = cogs::random(8000, 16384); // flawfinder: ignore
    }
    if (wind_zones[wind_zone] > 5000)
    {
      {
        wind_zones[wind_zone] -= 40;
      }
    }
  }

  // hide obvious patterns by randomizing the bin.
  index = (index * 134775813) % 16;

  if (millis() - last_flicker_run[index] > 20)
  {
    last_flicker_run[index] = millis();
    if (cogs::random(0, 100000) < wind_zones[wind_zone]) // flawfinder: ignore
    {
      // Always less
      flicker_flames[index] = cogs::random(flicker_flames[index] / 4, flicker_flames[index]); // flawfinder: ignore
    }
    else
    {
      if (flicker_flames[index] < 15947)
      {
        flicker_flames[index] += cogs::random(0, 1000); // flawfinder: ignore
      }
    }

    flicker_flames_lp[index] *= 7;
    flicker_flames_lp[index] += flicker_flames[index];
    flicker_flames_lp[index] = flicker_flames_lp[index] / 8;
  }

  // Todo maybe we should do float math the whole time
  return (float)flicker_flames_lp[index] / FXP_RES;
}

static float terenery(float a, float b, float c)
{
  if (a > 0)
  {
    return c;
  }
  else
  {
    return b;
  }
}

namespace cogs_rules
{
  std::map<std::string, float *> constants;
  std::map<std::string, float (*)()> user_functions0;
  std::map<std::string, float (*)(float)> user_functions1;
  std::map<std::string, float (*)(float, float)> user_functions2;
  std::map<std::string, float (*)(float, float, float)> user_functions3;

  void addUserFunction0(const std::string &name, float (*f)())
  {
    cogs::logErrorIfBadIdentifier(name);
    user_functions0[name] = f;
  }
  void addUserFunction1(const std::string &name, float (*f)(float))
  {
    cogs::logErrorIfBadIdentifier(name);
    user_functions1[name] = f;
  }
  void addUserFunction2(const std::string &name, float (*f)(float, float))
  {
    cogs::logErrorIfBadIdentifier(name);
    user_functions2[name] = f;
  }
  void addUserFunction3(const std::string &name, float (*f)(float, float, float))
  {
    cogs::logErrorIfBadIdentifier(name);
    user_functions3[name] = f;
  }

}

// Used when you want to make something change.
// Always returns positive int different from input.
float fbang(float x)
{
  int v = x;
  v += 1;
  if (v > 1048576)
  {
    return 1;
  }
  if (v < 1)
  {
    return 1;
  }
  return v;
}

void setupBuiltins()
{
  cogs_rules::constants["$res"] = &fxp_res_var;
  cogs_rules::constants["$i"] = &dollar_sign_i;
  cogs_rules::user_functions1["analogRead"] = &adcUserFunction;
  cogs_rules::user_functions0["random"] = &randomUserFunction;
  cogs_rules::user_functions0["millis"] = &timeUserFunction;
  cogs_rules::user_functions1["flicker"] = &doFlicker;
  cogs_rules::user_functions1["bang"] = &fbang;
  cogs_rules::user_functions0["uptime"] = &uptimeFunction;
  cogs_rules::user_functions3["switch"] = &terenery;

  cogs_rules::user_functions3["clamp"] = &clamp;

  cogs_rules::user_functions2["min"] = &min;
  cogs_rules::user_functions2["max"] = &max;
}

void cogs_rules::refreshBindingsEngine()
{
  clear_globals();

  for (const auto &[key, tag] : cogs_rules::constants)
  {
    append_global(key, tag);
  }

  for (const auto &[key, f] : cogs_rules::user_functions0)
  {
    append_func(key, reinterpret_cast<void *>(f), TE_FUNCTION0);
  }

  for (const auto &[key, f] : cogs_rules::user_functions1)
  {
    append_func(key, reinterpret_cast<void *>(f), TE_FUNCTION1);
  }

  for (const auto &[key, f] : cogs_rules::user_functions2)
  {
    append_func(key, reinterpret_cast<void *>(f), TE_FUNCTION2);
  }

  for (const auto &[key, f] : cogs_rules::user_functions3)
  {
    append_func(key, reinterpret_cast<void *>(f), TE_FUNCTION3);
  }

  for (const auto &tag : IntTagPoint::all_tags)
  {

    /// All variable access just reads the first value, sine tinyexpr doesn't support
    /// Arrays at all.
    append_global(tag->name, &tag->floatFirstValueCache);
  }

  int p = 0;
  while (global_vars[p].name)
  {
    p++;
  }

  global_vars_count = p;
  cogs_rules::needRefresh = false;
};

IntFadeClaim::IntFadeClaim(uint16_t startIndex, uint16_t count) : cogs_tagpoints::TagPointClaim(startIndex, count) {

                                                                  };

void IntFadeClaim::applyLayer(int32_t *vals, uint16_t tagLength)
{

  // Calculate how far along we are as a 0 to 100% control value
  int32_t blend_fader = millis() - this->start;
  if (blend_fader >= this->duration)
  {
    this->fadeDone = true;
    blend_fader = FXP_RES;
  }
  else
  {
    blend_fader *= FXP_RES;
    blend_fader = blend_fader / this->duration;
  }

  blend_fader *= this->alpha;

  blend_fader = blend_fader / FXP_RES;

  for (int i = 0; i < this->count; i++)
  {
    int mapped_idx = (this->startIndex + i);
    // Do not exceed tag bounds
    if (mapped_idx >= tagLength)
    {
      break;
    }

    int64_t blend_bg = vals[this->startIndex + i];
    blend_bg *= (FXP_RES - blend_fader);

    int64_t blend_fg = this->value[i];
    blend_fg *= (blend_fader);

    int64_t res = blend_bg + blend_fg;

    vals[this->startIndex + i] = res / FXP_RES;
  }
};

Binding *b = nullptr;

Binding::Binding(const std::string &target_name, const std::string &input, const int start, const int count)
{
  // Look for something like [1:4] at the end of the target, which would make this
  // A multi-binding.
  MatchState ms;

  if (target_name.size() > 255)
  {
    throw std::runtime_error("Target name too long");
  }

  char tn[256];                    // flawfinder: ignore
  strcpy(tn, target_name.c_str()); // flawfinder: ignore

  ms.Target(tn);
  this->target_name = target_name;
  b = this;

  this->multiStart = start;
  this->multiCount = count;

  this->inputExpressionSource = input;
  this->inputExpression = compileExpression(input);

  if (!this->target_name.size())
  {
    this->target = NULL;
  }
  else
  {
    this->trySetupTarget();
  }
  if (!this->target)
  {
    cogs::logError("No target: " + target_name);
  }
  if (!this->inputExpression)
  {
    cogs::logError("Bad expression: " + input);
  }

  this->lastState = reinterpret_cast<int *>(malloc(sizeof(int) * this->multiCount));

  this->stateUnknown = reinterpret_cast<bool *>(malloc(sizeof(bool) * this->multiCount));
  this->ready = true;

  if (!this->lastState || !this->stateUnknown)
  {
    cogs::logError("Memory allocation failed");
    this->ready = false;
  }
};

bool Binding::trySetupTarget()
{
  if (IntTagPoint::exists(this->target_name))
  {
    this->target = IntTagPoint::getTag(this->target_name, 0, 1);

    if (this->multiCount == 0)
    {
      this->multiCount = target->count;
    }

    if (this->fadeInTime || this->alpha || this->layer)
    {
      if (!this->claim)
      {
        this->claim = std::make_shared<cogs_rules::IntFadeClaim>(
            this->multiStart,
            this->multiCount);
      }

      int l = this->layer;
      if (this->fadeInTime || this->alpha)
      {
        if (l < 1)
        {
          l = 1;
        }
      }

      this->claim->priority = l;
    }
    return true;
  }

  return false;
}
void Binding::eval()
{

  if (!this->ready)
  {
    return;
  }
  if (!this->inputExpression)
  {
    return;
  }

  // We may be setting multiple values, we want to only do one rerender pass
  // for performance reasons.

  // Maybe the tag was made after the binding.
  if (!this->target)
  {
    if (!this->trySetupTarget())
    {
      return;
    }
  }

  bool shouldRerender = false;

  if (this->claim)
  {
    if (!this->claim->fadeDone)
    {
      shouldRerender = true;
    }
    else
    {
      if (!this->claim->finished)
      {
        if (this->layer == 0)
        {
          shouldRerender = true;
          this->claim->finished = true;
        }
      }
    }
  }

  if (!this->frozen)
  {
    dollar_sign_i = this->multiStart;

    // If it's a multi binding, evaluate each part
    for (int i = 0; i < this->multiCount; i++)
    {

      float x = te_eval(this->inputExpression);

      // If the conditions are met, the val we want to assign
      float new_v = x;

      bool should_set = true;

      if (this->trigger_mode)
      {
        // floor
        x = floor(x);

        if (x > 0.0)
        {
          // Can do this all with ints!
          new_v = cogs::bang(this->target->value[this->multiStart + i] / this->target->scale);
        }
        else
        {
          should_set = false;
        }
      }

      if (this->onchange)
      {

        if (x != this->lastState[i])
        {

          /// nan is the special flag meaning we are in an unknown state
          /// And thus cannot do change detection yet, so we don't
          /// Act until it changes again.

          // If onenter is true, we act on enter no matter what
          if ((!this->stateUnknown[i]) || this->onenter)
          {
            if (should_set)
            {
              if (this->claim)
              {
                if (!this->claim->finished)
                {
                  this->claim->value[this->multiStart + i] = new_v * this->target->scale;
                }
                else
                {
                  this->target->background_value[this->multiStart + i] = new_v * this->target->scale;
                }
              }
              else
              {
                this->target->background_value[this->multiStart + i] = new_v * this->target->scale;
              }
              shouldRerender = true;
            }
          }
          this->lastState[i] = x;
        }

        this->stateUnknown[i] = false;
      }
      else
      {
        if (should_set)
        {
          if (this->claim)
          {
            if (!this->claim->finished)
            {
              this->claim->value[this->multiStart + i] = new_v * this->target->scale;
            }
            else
            {
              this->target->background_value[this->multiStart + i] = new_v * this->target->scale;
            }
          }
          else
          {
            this->target->background_value[this->multiStart + i] = new_v * this->target->scale;
          }
          shouldRerender = true;
        }
      }

      dollar_sign_i++;
    }

    // Set it back to 0
    dollar_sign_i = 0;

    if (this->freeze)
    {
      this->frozen = true;
    }
  }

  if (shouldRerender)
  {
    this->target->rerender();
  }
};

Binding::~Binding()
{
  if (this->inputExpression)
  {
    te_free(this->inputExpression);
  }
  if (this->fadeInTime)
  {
    te_free(this->fadeInTime);
  }

  if (this->lastState)
  {
    free(this->lastState);
    this->lastState = nullptr;
  }
  if (this->stateUnknown)
  {
    free(this->stateUnknown);
    this->stateUnknown = nullptr;
  }
};

void Binding::enter()
{
  this->frozen = false;
  for (int i = 0; i < this->multiCount; i++)
  {
    this->stateUnknown[i] = true;
  }

  // Almost all the actual claim config happens here, we just reuse stuff.
  if (this->claim)
  {
    this->claim->alpha = cogs_rules::FXP_RES;

    // Be, defensive, B-E defensive!
    if (this->fadeInTime)
    {
      this->claim->duration = te_eval(this->fadeInTime) * 1000;
    }
    else
    {
      this->claim->duration = 0;
    }

    this->claim->start = millis();
    this->claim->fadeDone = false;
    this->claim->finished = false;

    if (this->target)
    {
      this->target->addClaim(this->claim);
    }
  }
}

void Binding::exit()
{
  // We don't need that claim anymore, merge it back into the background state if possible.
  if (this->claim)
  {

    // Layers don't get folded into the background.
    // They go away when we're done so we can use them as overrides.
    if (this->layer == 0)
    {
      this->claim->finished = true;
    }
    else
    {
      this->target->removeClaim(this->claim);
    }

    if (this->target)
    {
      // Needed to properly clean the finished claims.
      this->target->rerender();
    }
  }
}

void State::eval()
{
  for (const auto &binding : this->bindings)
  {
    binding->eval();
  }
}

void State::enter()
{
  for (const auto &binding : this->bindings)
  {
    binding->enter();
  }
}

void State::exit()
{
  for (const auto &binding : this->bindings)
  {
    binding->exit();
  }
}

std::shared_ptr<Binding> State::addBinding(std::string target_name, std::string input,
                                           int start, int count)
{
  std::shared_ptr<Binding> binding = std::make_shared<Binding>(target_name, input, start, count);
  this->bindings.push_back(binding);

  return binding;
}

void State::removeBinding(std::shared_ptr<Binding> binding)
{
  /// remove all elements in this->bindings which are equal to binding
  this->bindings.erase(std::remove(this->bindings.begin(),
                                   this->bindings.end(), binding),
                       this->bindings.end());
}

void State::clearBindings()
{
  this->bindings.clear();
}

State::~State()
{
  if (this->tag)
  {
    this->tag->unsubscribe(&onStateTagSet);
    this->tag->unregister();
    this->tag = nullptr;
  }
}
std::shared_ptr<Clockwork> Clockwork::getClockwork(std::string name)
{
  if (Clockwork::allClockworks.count(name) == 1)
  {
    return Clockwork::allClockworks[name];
  }
  Clockwork *cwp = new Clockwork(name);

  std::shared_ptr<Clockwork> cw(cwp);
  Clockwork::allClockworks[name] = cw;
  return cw;
}

void Clockwork::evalAll()
{
  for (const auto &cw : Clockwork::allClockworks)
  {
    cw.second->eval();
  }
}
void Clockwork::close()
{
  Clockwork::allClockworks.erase(this->name);
}

Clockwork::Clockwork(const std::string &name)
{
  if (name.size() == 0)
  {
    throw std::runtime_error("name empty");
  }
  this->name = name;
  this->enteredAt = millis();
}

void Clockwork::gotoState(const std::string &name, unsigned long time)
{
  cogs::logInfo("Clockwork " + this->name + " gotoState " + name);

  if (this->currentState)
  {
    std::string stateName = this->currentState->name;
    auto tag = IntTagPoint::getTag(this->name + ".states." + stateName, 0);
    // If re-entering the same state, don't reset it
    // that would trigger an infinite loop with the tag point making us re-enter
    if (!(name == stateName))
    {
      tag->setValue(0);
    }
    this->currentState->exit();
  }

  if (!(this->states.count(name) == 1))
  {
    cogs::logError("Clockwork " + this->name + " doesn't have state " + name);
    return;
  }

  this->currentState = this->states[name].get();

  // Mark to not make a loop and enter an extra time
  this->currentState->lastTagValue = 1;

  // Update the state tags
  auto tag2 = IntTagPoint::getTag(this->name + ".states." + name, 1);
  tag2->setValue(1);


  if (time == 0)
  {
    this->enteredAt = millis();
  }
  else
  {
    this->enteredAt = time;
  }

  // Reset change detection state
  if (this->currentState)
  {
    this->currentState->enter();
  }
}

Clockwork::~Clockwork()
{
  // These can't be auto cleaned because there's a global list.
  for (const auto &tag : this->tags)
  {
    tag->unregister();
  }
}

// The GIL should make this ok
int recursionLimit = 0;

static void onStateTagSet(IntTagPoint *tag)
{

  State *state = static_cast<State *>(tag->extraData);

  // We do our own change detect logic so we can set this var when we want to ignore stuff
  if (state->lastTagValue == tag->value[0])
  {
    return;
  }
  
  recursionLimit++;

  state->lastTagValue = tag->value[0];

  if (recursionLimit > 2)
  {
    cogs::logError("State tag set recursion limit exceeded going to " + state->name);
    recursionLimit = 0;
    return;
  }

  if (state)
  {
    if (tag->value[0])
    {
      cogs::logInfo("Tag causing transition " + state->owner->name + " gotoState " + state->name);
      state->owner->gotoState(state->name);
    }
    state->eval();
  }

  recursionLimit--;
}

std::shared_ptr<State> Clockwork::getState(std::string name)
{
  if (name.size() == 0)
  {
    throw std::runtime_error("name empty");
  }

  if (this->states.count(name) == 1)
  {
    return this->states[name];
  }

  std::shared_ptr<State> state = std::make_shared<State>();

  state->name = name;

  // This is the tag point which will be set to 1 if we're in this state
  // We can't be in it yet since we just created it
  auto tag = IntTagPoint::getTag(this->name + ".states." + name, 0);
  tag->setUnit("bool");

  tag->subscribe(&onStateTagSet);

  tag->extraData = state.get();

  state->tag = tag;

  this->states[name] = state;

  state->owner = Clockwork::allClockworks[this->name];
  return state;
}

void Clockwork::removeState(std::string name)
{
  // Break the ref loop
  this->states[name]->owner = NULL;
  this->states.erase(name);
}

void Clockwork::eval()
{
  ctx_cw = this;
  if (this->currentState)
  {

    // handle duration logic
    if (this->currentState->duration > 0)
    {
      if ((millis() - this->enteredAt) > this->currentState->duration)
      {
        if (this->currentState->nextState.length() > 0)
        {
          cogs::logInfo("Clockwork " + this->name + " timer transition to " + this->currentState->nextState);
          this->gotoState(this->currentState->nextState);
        }
      }
      this->currentState->eval();
    }
    else
    {
      this->currentState->eval();
    }
  }
}

static void fastPoll()
{
  if (cogs_rules::needRefresh)
  {
    cogs_rules::refreshBindingsEngine();
  }
  cogs_rules::Clockwork::evalAll();
}

static void printTagValue(std::shared_ptr<cogs_rules::IntTagPoint> tag, reggshell::Reggshell *rs)
{

  rs->print("Tag Value: ");

  for (int i = 0; i < 32; i++)
  {
    if (i >= tag->count)
    {
      break;
    }
    rs->print(tag->value[i] / tag->scale);
    rs->print(", ");
  }

  rs->println("");

  if (tag->scale)
  {
    rs->print("Raw: ");
    for (int i = 0; i < 32; i++)
    {
      if (i >= tag->count)
      {
        break;
      }
      rs->print(tag->value[i]);
      rs->print(", ");
    }
    rs->println("");
  }
}
/// Reggshell command to read a tag point

static void reggshellReadTagPoint(reggshell::Reggshell *rs, const char *arg1, const char *arg2, const char *arg3)
{
  if (cogs_rules::IntTagPoint::exists(arg1))
  {
    auto tag = cogs_rules::IntTagPoint::getTag(arg1, 0);
    if (tag)
    {
      printTagValue(tag, rs);
    }
  }
}

static void reggshellWriteTagPoint(reggshell::Reggshell *rs, const char *arg1, const char *arg2, const char *arg3)
{
  if (cogs_rules::IntTagPoint::exists(arg1))
  {
    auto tag = cogs_rules::IntTagPoint::getTag(arg1, 0);
    if (tag)
    {
      float val = atof(arg2); // flawfinder: ignore
      tag->setValue(val * tag->scale);
      tag->rerender();

      printTagValue(tag, rs);
    }
  }
}

static void statusCallback(reggshell::Reggshell *rs)
{
  rs->println("\nClockworks: ");
  for (auto const &cw : cogs_rules::Clockwork::allClockworks)
  {
    rs->print("  ");
    rs->print(cw.first.c_str());
    rs->print(": ");
    if (cw.second->currentState == NULL)
    {
      rs->println("null");
    }
    else
    {
      rs->print("   ");
      rs->println(cw.second->currentState->name.c_str());
    }
  }
}

static void listTagsCommand(reggshell::Reggshell *rs, const char *arg1, const char *arg2, const char *arg3)
{
  for (auto const &tag : cogs_rules::IntTagPoint::all_tags)
  {
    rs->print(tag->name.c_str());
    rs->print("[");
    rs->print(std::to_string(tag->count).c_str());
    rs->print("]: ");

    rs->print((float)tag->value[0] / (float)tag->scale);
    rs->println(tag->unit->c_str());
  }
}

static void evalExpressionCommand(reggshell::Reggshell *rs, MatchState *ms, const char *rawline)
{
  char buf[256]; // flawfinder: ignore
  ms->GetCapture(buf, 0);
  try
  {
    rs->println(cogs_rules::evalExpression(std::string(buf)));
  }
  catch (std::exception &e)
  {
    rs->println(e.what());
  }
}
void refreshHandler(cogs::GlobalEvent evt, int dummy, const std::string &filename)
{
  if (evt == cogs::tagCreatedEvent || evt == cogs::tagDestroyedEvent)
  {
    cogs_rules::needRefresh = true;
  }
  if (evt == cogs::bindingsEngineRefreshEvent)
  {
    {
      bool good = true;
      for (const auto &cw : Clockwork::allClockworks)
      {
        if (!cw.second->handleEngineRefresh())
        {
          good = false;
        }
      }
      if (!good)
      {
        cogs::logError("Clockwork refresh failed");
        cogs::addTroubleCode("EBADAUTOMATION");
      }
    }
  }
}

auto ram = IntTagPoint::getTag("sys.memfree", 0);

static void slowPoll()
{
#if defined(ESP32) || defined(ESP8266)
  ram->smartSetValue(ESP.getFreeHeap(), 2048, 600000);
#endif
}

void cogs_rules::begin()
{
  if (cogs_rules::started)
  {
    cogs::logError("Rules already started");
    cogs::addTroubleCode("ESETUPORDER");
    return;
  }
  cogs::mainThreadHandle = xTaskGetCurrentTaskHandle();

  setupBuiltins();

  ram->setUnit("bytes");

  cogs::slowPollHandlers.push_back(slowPoll);

  cogs_rules::user_functions0["age"] = &getStateAge;

  cogs_reggshell::interpreter->addCommand("eval (.*)", evalExpressionCommand, "eval <expr> Evaluates any expression. All tag points available as vars");
  cogs_reggshell::interpreter->addSimpleCommand("tags", listTagsCommand, "list all tags and their first vals");
  cogs_reggshell::interpreter->addSimpleCommand("get", reggshellReadTagPoint, "get <tag> reads a tag point value, up to the first 32 vals");
  cogs_reggshell::interpreter->addSimpleCommand("set", reggshellWriteTagPoint, "set <tag> <value> sets a tag value.  \nIf value is a floating point, it will be multiplied by $res=16384.\n If tag is array, sets every element.");
  cogs_reggshell::interpreter->statusCallbacks.push_back(statusCallback);
  cogs::fastPollHandlers.push_back(fastPoll);
  cogs::globalEventHandlers.push_back(&refreshHandler);
  refreshBindingsEngine();

  cogs_rules::started = true;
}

bool Clockwork::handleEngineRefresh()
{
  bool good = true;
  for (const auto &state : this->states)
  {

    if (!state.second->handleEngineRefresh())
    {
      good = false;
    }
  }
  return good;
}

bool Binding::handleEngineRefresh()
{
  bool good = true;

  if (this->inputExpression)
  {
    te_free(this->inputExpression);
    this->inputExpression = compileExpression(this->inputExpressionSource);
    if (!this->inputExpression)
    {
      this->inputExpression = nullptr;
      cogs::logError("Error compiling expression: " + this->inputExpressionSource);
      good = false;
    }
  }

  if (this->fadeInTime)
  {
    te_free(this->fadeInTime);
    this->fadeInTime = compileExpression(this->fadeInTimeSource);
    if (!this->fadeInTime)
    {
      this->fadeInTime = nullptr;
      cogs::logError("Error compiling expression: " + this->fadeInTimeSource);
      good = false;
    }
  }

  if (this->alpha)
  {
    te_free(this->alpha);
    this->alpha = compileExpression(this->alphaSource);
    if (!this->alpha)
    {
      this->alpha = nullptr;
      cogs::logError("Error compiling expression: " + this->alphaSource);
      good = false;
    }
  }

  if (this->target)
  {
    this->target = nullptr;
    this->trySetupTarget();
  }

  return good;
}

bool State::handleEngineRefresh()
{
  bool good = true;
  for (const auto &binding : this->bindings)
  {
    if (!binding->handleEngineRefresh())
    {
      good = false;
    }
  }

  return good;
}
