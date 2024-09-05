#include <Arduino.h>
#include "cogs_bindings_engine.h"
#include "cogs_util.h"

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
      free((void *)global_vars[i].name);
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
static void append_global(std::string name, const int32_t *value)
{
  // Make a C compatible version of the string we need to manually free later
  char *n = (char *)malloc(name.size() + 1);
  n[name.size()] = (char)NULL;
  memcpy(n, name.c_str(), name.size());

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

void cogs_rules::refresh_bindings_engine()
{
  clear_globals();

  // We check and don't let user target anything starting with $.
  append_global("$RES", &fxp_res_var);

  for (const auto &[key, tag] : IntTagPoint::all_tags)
  {
    append_global(key, &(tag->last_calculated_value));
  }
};

IntFadeClaim::IntFadeClaim(unsigned long start, unsigned long duration, int32_t value, uint16_t alpha)
{
  this->start = start;
  this->duration = duration;
  this->value = value;
  this->alpha = alpha;
  this->finished = false;
};

int32_t IntFadeClaim ::applyLayer(int32_t bg)
{

  // Calculate how far along we are as a 0 to 100% control value
  int32_t blend_fader = millis() - this->start;
  if (blend_fader > this->duration)
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

  int64_t blend_bg = bg;
  blend_bg *= (FXP_RES - blend_fader);

  int64_t blend_fg = this->value;
  blend_fg *= (FXP_RES - blend_fader);

  int64_t res = blend_bg + blend_fg;

  return res / FXP_RES;
}

Binding::Binding(std::string target_name, std::string input)
{
  int err = 0;

  this->input_expression = te_compile(input.c_str(), global_vars, global_vars_count, &err);
  if (err)
  {
    input_expression = NULL;
  }

  if (target_name.size() && target_name[0] == '$')
  {
    this->target = NULL;
  }
  else
  {

    if (IntTagPoint::all_tags.contains(target_name))
    {
      this->target = IntTagPoint::all_tags[target_name];
    }
  }
};

void Binding::eval()
{
  if (this->input_expression)
  {
    float x = te_eval(this->input_expression);

    // Change detection, don't send on repeats.
    if (x == this->last_value)
    {
      return;
    }

    this->last_value = x;

    if (this->target)
    {
      this->target->setValue(x);
    }
  }
};

Binding::~Binding()
{
  te_free(this->input_expression);
};

void Binding::reset()
{
  // This is our no data state.
  this->last_value = -NAN;
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

static std::shared_ptr<Clockwork> getClockwork(std::string name)
{
  if (Clockwork::allClockworks.contains(name))
  {
    return Clockwork::allClockworks[name];
  }
  Clockwork * cwp = new Clockwork(name);

  std::shared_ptr<Clockwork> cw(cwp);
  Clockwork::allClockworks[name] = cw;
  return cw;
}

static void evalAll()
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

Clockwork::Clockwork(std::string name)
{
  this->name = name;
  this->currentState = "default";
  this->enteredAt = millis();
}

void Clockwork::gotoState(std::string name)
{
  // Update the state tags
  auto tag = IntTagPoint::getTag(this->name + ".states." + this->currentState, 0);
  tag->setValue(0);
  auto tag2 = IntTagPoint::getTag(this->name + ".states." + name, 1);
  tag2->setValue(1);

  this->currentState = name;
  this->enteredAt = millis();
}

Clockwork::~Clockwork()
{
  // These can't be auto cleaned because there's a global list.
  for (const auto &tag : this->tags)
  {
    tag->unregister();
  }
}

static void onStateTagSet(int32_t value, IntTagPoint *tag)
{
  State *state = (State *)tag->extraData;
  if (state)
  {
    if (value)
    {
      state->owner->gotoState(state->name);
    }
    state->eval();
  }
}

std::shared_ptr<State> Clockwork::getState(std::string name)
{
  if (this->states.contains(name))
  {
    return this->states[name];
  }

  std::shared_ptr<State> state = std::make_shared<State>();

  // This is the tag point which will be set to 1 if we're in this state
  bool is_in_state = (this->currentState == name);
  int v = is_in_state ? 1 : 0;
  auto tag = IntTagPoint::getTag(this->name + ".states." + name, v);

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
  if (this->states.contains(this->currentState))
  {
    auto state = this->states[this->currentState];

    // handle duration logic
    if (state->duration)
    {
      if (millis() - this->enteredAt > state->duration)
      {
        if (state->nextState.length() > 0)
        {
          this->gotoState(state->nextState);
        }
      }
      this->states[this->currentState]->eval();
    }
    else
    {
      state->eval();
    }
  }
  else
  {
    cogs::logError("State not found: " + this->currentState);
  }
}