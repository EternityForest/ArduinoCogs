#include <Arduino.h>
#include <math.h>
#include "cogs_bindings_engine.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include <Regexp.h>
#include "reggshell/reggshell.h"
#include "cogs_reggshell.h"

using namespace cogs_rules;

static std::vector<Binding> bindings;

/// This is a set of tinyexpr variables.
/// Note that we have modified tinyexpr so values are not floats,
/// We do this because we want to use fixed point internally and we do not know what vars will
/// Be requested until they actually are.
static int global_vars_count = 0;
static te_variable global_vars[256];

te_expr * cogs_rules::compileExpression(const std::string &input)
{
  int err = 0;
  auto x = te_compile(input.c_str(), global_vars, global_vars_count, &err);
  if (err)
  {
    throw std::runtime_error("Failed to compile binding" + input + " error:" + std::to_string(err));
  }
  return x;
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

static float randomUserFunction()
{
  uint16_t r = cogs::random(); // flawfinder: ignore
  return (float)r / 65535.0f;
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

  return flicker_flames[index];
}

namespace cogs_rules
{
  std::map<std::string, float *> constants;
  std::map<std::string, float (*)()> user_functions0;
  std::map<std::string, float (*)(float)> user_functions1;
  std::map<std::string, float (*)(float)> user_functions2;
}

// Used when you want to make something change.
// Always returns positive int different from input.
float fbang(float x){
  int v = x;
  v+=1;
  if(v>1048576){
    return 1;
  }
  if(v<1){
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
};

void IntFadeClaim::applyLayer(int32_t *vals, int tagLength)
{

  // Calculate how far along we are as a 0 to 100% control value
  int32_t blend_fader = millis() - this->start;
  if (blend_fader >= this->duration)
  {
    blend_fader = FXP_RES;
    this->finished = true;
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
    blend_fg *= (FXP_RES - blend_fader);

    int64_t res = blend_bg + blend_fg;

    vals[this->startIndex + i] = res / FXP_RES;
  }
};

Binding::Binding(const std::string &target_name, const std::string &input)
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
  if (ms.Match("\\[([0-9]+):([0-9]+)\\]") == REGEXP_MATCHED)
  {
    char c[256]; // flawfinder: ignore
    ms.GetCapture(c, 1);
    this->multiStart = atoi(c); // flawfinder: ignore
    ms.GetCapture(c, 2);
    this->multiCount = atoi(c) - this->multiStart; // flawfinder: ignore

    // Remove the multi specifier part from the target
    this->target_name = target_name.substr(0, target_name.find('['));
  }
  else
  {
    this->target_name = target_name;
  }

  this->input_expression = compileExpression(input);

  if (this->target_name.size() && this->target_name[0] == '$')
  {
    this->target = NULL;
  }
  else
  {
    if (IntTagPoint::exists(this->target_name))
    {
      this->target = IntTagPoint::getTag(this->target_name,0,1);
    }
  }

  this->lastState = reinterpret_cast<int *>(malloc(sizeof(int) * this->multiCount));
};

void Binding::eval()
{
  if (this->input_expression)
  {
    // If it's a multi binding, evaluate each part
    for (int i = 0; i < this->multiCount; i++)
    {

      if (this->multiCount > 1)
      {
        // User code will likely want to access the iterator $i
        dollar_sign_i = this->multiStart + i;
      }

      float x = te_eval(this->input_expression);

      // Maybe the tag was made after the binding.
      if (!this->target)
      {
        if (IntTagPoint::exists(this->target_name))
        {
          this->target = IntTagPoint::getTag(this->target_name,0,1);
        }
      }

      if (this->target)
      {
        if (this->frozen)
        {
        }
        else if (this->onchange)
        {
          if (x != this->lastState[i])
          {

            /// nan is the special flag meaning we are in an unknown state
            /// And thus cannot do change detection yet, so we don't
            /// Act until it changes again.

            // If onenter is true, we act on enter no matter what
            if ((this->lastState[i] != NAN) || this->onenter)
            {
              this->target->setValue(x * this->target->scale, this->multiStart + i, 1);
            }
            this->lastState[i] = x;
          }
        }
        else
        {
          this->target->setValue(x * this->target->scale, this->multiStart + i, 1);
        }

        if (this->freeze)
        {
          this->frozen = true;
        }
      }
    }
    // Set it back to 0
    dollar_sign_i = 0;
  }
};

Binding::~Binding()
{
  te_free(this->input_expression);
};

void Binding::reset()
{
  this->frozen = false;
  for (int i = 0; i < this->multiCount; i++)
  {
    this->lastState[i] = 0;
  }
}

void State::eval()
{
  for (const auto &binding : this->bindings)
  {
    binding->eval();
  }
}

void State::reset()
{
  for (const auto &binding : this->bindings)
  {
    binding->reset();
  }
}

std::shared_ptr<Binding> State::addBinding(std::string target_name, std::string input)
{
  std::shared_ptr<Binding> binding = std::make_shared<Binding>(target_name, input);
  this->bindings.push_back(binding);

  return binding;
}

void State::removeBinding(std::shared_ptr<Binding> binding)
{
  std::erase(this->bindings, binding);
}

void State::clearBindings()
{
  this->bindings.clear();
}

std::shared_ptr<Clockwork> Clockwork::getClockwork(std::string name)
{
  if (Clockwork::allClockworks.contains(name))
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

  if (this->currentState)
  {
    std::string stateName = this->currentState->name;
    auto tag = IntTagPoint::getTag(this->name + ".states." + stateName, 0);
    tag->setValue(0);
  }

  if (!this->states.contains(name))
  {
    cogs::logError("Clockwork " + this->name + " doesn't have state " + name);
    return;
  }

  // Update the state tags
  auto tag2 = IntTagPoint::getTag(this->name + ".states." + name, 1);
  tag2->setValue(1);

  this->currentState = this->states[name].get();

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
    this->currentState->reset();
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

static void onStateTagSet(IntTagPoint *tag)
{
  State *state = static_cast<State *>(tag->extraData);
  if (state)
  {
    if (tag->value[0])
    {
      state->owner->gotoState(state->name);
    }
    state->eval();
  }
}

std::shared_ptr<State> Clockwork::getState(std::string name)
{
  if (name.size() == 0)
  {
    throw std::runtime_error("name empty");
  }

  if (this->states.contains(name))
  {
    return this->states[name];
  }

  std::shared_ptr<State> state = std::make_shared<State>();

  state->name = name;

  // This is the tag point which will be set to 1 if we're in this state
  // We can't be in it yet since we just created it
  auto tag = IntTagPoint::getTag(this->name + ".states." + name, 0);

  tag->subscribe(&onStateTagSet);

  tag->extraData = state.get();

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
  if (this->currentState)
  {

    // handle duration logic
    if (this->currentState->duration)
    {
      if (millis() - this->enteredAt > this->currentState->duration)
      {
        if (this->currentState->nextState.length() > 0)
        {
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
  for (auto const & cw : cogs_rules::Clockwork::allClockworks)
  {
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
  for (auto  const & tag : cogs_rules::IntTagPoint::all_tags)
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
    te_expr *expr = compileExpression(buf);

    if (expr)
    {
      rs->println(te_eval(expr));
      te_free(expr);
    }
  }
  catch (std::exception &e)
  {
    rs->println(e.what());
  }
}
void cogs_rules::setupRulesEngine()
{
  setupBuiltins();
  auto t = cogs_rules::IntTagPoint::getTag("temp1", 0);
  t->scale = 16384;
  t = cogs_rules::IntTagPoint::getTag("temp2", 0);
  t->scale = 16384;
  t = cogs_rules::IntTagPoint::getTag("temp3", 0);
  t->scale = 16384;

  cogs_reggshell::interpreter->addCommand("eval (.*)", evalExpressionCommand, "eval <expr> Evaluates any expression. All tag points available as vars");
  cogs_reggshell::interpreter->addSimpleCommand("tags", listTagsCommand, "list all tags and their first vals");
  cogs_reggshell::interpreter->addSimpleCommand("get", reggshellReadTagPoint, "get <tag> reads a tag point value, up to the first 32 vals");
  cogs_reggshell::interpreter->addSimpleCommand("set", reggshellWriteTagPoint, "set <tag> <value> sets a tag value.  \nIf value is a floating point, it will be multiplied by $res=16384.\n If tag is array, sets every element.");
  cogs_reggshell::interpreter->statusCallbacks.push_back(statusCallback);
  cogs::fastPollHandlers.push_back(fastPoll);
  refreshBindingsEngine();
}