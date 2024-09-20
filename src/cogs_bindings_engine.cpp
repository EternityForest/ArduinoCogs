#include <Arduino.h>
#include "cogs_bindings_engine.h"
#include "cogs_util.h"
#include "cogs_global_events.h"
#include <Regexp.h>
#include "reggshell/reggshell.h"
#include "cogs_reggshell.h"

using namespace cogs_rules;

static std::list<Binding> bindings;

/// This is a set of tinyexpr variables.
/// Note that we have modified tinyexpr so values are not floats,
/// We do this because we want to use fixed point internally and we do not know what vars will
/// Be requested until they actually are.
static int global_vars_count = 0;
static te_variable global_vars[256];

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
static void append_global(const std::string &name, const int32_t *value)
{
  // Make a C compatible version of the string we need to manually free later
  char *n = static_cast<char *>(malloc(name.size() + 1));


  n[name.size()] = (char)NULL;
  memcpy(n, name.c_str(), name.size()); //flawfinder: ignore

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

static int32_t fxp_res_var = FXP_RES;

static int32_t dollar_sign_i = 0;

void cogs_rules::refreshBindingsEngine()
{
  clear_globals();

  // We check and don't let user target anything starting with $.
  append_global("$RES", &fxp_res_var);

  // Used in iteration
  append_global("$i", &dollar_sign_i);

  for (const auto &[key, tag] : IntTagPoint::all_tags)
  {

    /// All variable access just reads the first value, sine tinyexpr doesn't support
    /// Arrays at all.
    append_global(key, tag->value);
  }
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
  int err = 0;

  // Look for something like [1:4] at the end of the target, which would make this
  // A multi-binding.
  MatchState ms;

  if(target_name.size()>255){
    throw std::runtime_error("Target name too long");
  }

  char tn[256]; //flawfinder: ignore
  strcpy(tn, target_name.c_str()); // flawfinder: ignore

  ms.Target(tn);
  if (ms.Match("\\[([0-9]+):([0-9]+)\\]") == REGEXP_MATCHED)
  {
    char c[256]; //flawfinder: ignore
    ms.GetCapture(c, 1);
    this->multiStart = atoi(c); //flawfinder: ignore
    ms.GetCapture(c, 2);
    this->multiCount = atoi(c) - this->multiStart; //flawfinder: ignore

    // Remove the multi specifier part from the target
    this->target_name = target_name.substr(0, target_name.find('['));
  }
  else{
    this->target_name = target_name;
  }

  this->input_expression = te_compile(input.c_str(), global_vars, global_vars_count, &err);
  if (err)
  {
    throw std::runtime_error("Failed to compile binding" + 
    input + " error:" + std::to_string(err));
  }

  if (this->target_name.size() && this->target_name[0] == '$')
  {
    this->target = NULL;
  }
  else
  {
    if (IntTagPoint::all_tags.contains(this->target_name))
    {
      this->target = IntTagPoint::all_tags[this->target_name];
    }
  }
};

void Binding::eval()
{
  if (this->input_expression)
  {
    Serial.println("eval");
    Serial.println(this->multiCount);
    // If it's a multi binding, evaluate each part
    for (int i = 0; i < this->multiCount; i++)
    {

      // User code will likely want to access the iterator $i
      dollar_sign_i = this->multiStart + i;

      float x = te_eval(this->input_expression);

      // Maybe the tag was made after the binding.
      if (!this->target)
      {
        if (IntTagPoint::all_tags.contains(this->target_name))
        {
          this->target = IntTagPoint::all_tags[this->target_name];
        }
      }

      if (this->target)
      {
        Serial.println(this->target_name.c_str());
        Serial.println(x);
        Serial.println(this->multiStart);
        Serial.println(i);
        this->target->setValue(x, this->multiStart + i, 1);
      }
    }
  }
};

Binding::~Binding()
{
  te_free(this->input_expression);
};

void Binding::reset()
{
  // // This is our no data state.
  // this->last_value = -NAN;
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
  this->bindings.remove(binding);
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
  if(name.size() == 0){
    throw std::runtime_error("Clockwork::getState() called with empty name");
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

/// Reggshell command to read a tag point

static void reggshellReadTagPoint(reggshell::Reggshell *rs, const char *arg1, const char *arg2, const char *arg3)
{
  if (cogs_rules::IntTagPoint::exists(arg1))
  {
    auto tag = cogs_rules::IntTagPoint::getTag(arg1, 0);
    if (tag)
    {
      tag->rerender();
      rs->print("Tag Value: ");

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
}
static void statusCallback(reggshell::Reggshell *rs){
  rs->println("\nClockworks: ");
  for(auto cw : cogs_rules::Clockwork::allClockworks){
    rs->print(cw.first.c_str());
    rs->print(": ");
    if(cw.second->currentState == NULL){
      rs->println("null");
    }
    else
    {
      rs->println(cw.second->currentState->name.c_str());
    }
  }
}

void cogs_rules::setupRulesEngine()
{
  cogs_reggshell::interpreter->addSimpleCommand("get", reggshellReadTagPoint, "Read a tag point value, up to the first 32 vals");
  cogs_reggshell::interpreter->statusCallbacks.push_back(statusCallback);
  cogs::fastPollHandlers.push_back(fastPoll);
}