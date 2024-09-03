#include "cogs_bindings_engine.h"

using namespace cogs_rules;

static std::list<Binding> bindings;




/// This is a set of tinyexpr variables.
/// Note that we have modified tinyexpr so values are not floats,
/// We do this because we want to use fixed point internally and we do not know what vars will
/// Be requested until they actually are.
static int global_vars_count = 0;
static te_variable global_vars[256];



static void clear_globals() {
  int i = 0;
  while (1) {
    if (global_vars[i].name) {
      // We put it there and know this isn't really const, it's dynamically malloced.
      free((void*)global_vars[i].name);
      global_vars[i].name = 0;
      global_vars[i].address = 0;
      global_vars[i].context = 0;
      global_vars[i].type = 0;
      i++;
    } else {
      break;
    }
  }
};

// Add a new global variable.
static void append_global(std::string name, const int32_t* value) {
  // Make a C compatible version of the string we need to manually free later
  char* n = (char*)malloc(name.size() + 1);
  n[name.size()] = (char)NULL;
  memcpy(n, name.c_str(), name.size());

  int i = 0;
  while (1) {
    if (global_vars[i].name == NULL) {
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

void cogs_rules::refresh_bindings_engine() {
  clear_globals();

  // We check and don't let user target anything starting with $.
  append_global("$RES", &fxp_res_var);


  for (const auto& [key, tag] : IntTagPoint::all_tags) {
    append_global(key, &(tag->last_calculated_value));
  }
};



IntFadeClaim::IntFadeClaim(unsigned long start, unsigned long duration, int32_t value, uint16_t alpha) {
  this->start = start;
  this->duration = duration;
  this->value = value;
  this->alpha = alpha;
  this->finished = false;
};

int32_t IntFadeClaim ::applyLayer(int32_t bg) {

  // Calculate how far along we are as a 0 to 100% control value
  int32_t blend_fader = millis() - this->start;
  if (blend_fader > this->duration) {
    blend_fader = FXP_RES;
    this->finished = true;
  } else {
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








Binding::Binding(std::string target_name, std::string input) {
  int err = 0;

  this->input_expression = te_compile(input.c_str(), global_vars, global_vars_count, &err);
  if (err) {
    input_expression = NULL;
  }

  if(target_name.size() && target_name[0] == '$') {
    this->target = NULL;
  }
  else{

    if (IntTagPoint::all_tags.contains(target_name)) {
      this->target = IntTagPoint::all_tags[target_name];
    }

  }


};

void Binding::eval() {
  if (this->input_expression) {
    float x = te_eval(this->input_expression);

    // Change detection, don't send on repeats.
    if(x == this->last_value) {
      return;
    }

    this->last_value = x;

    if (this->target) {
      this->target->setValue(x);
    }
  }
};

Binding::~Binding() {
  te_free(this->input_expression);
};


void Binding::reset() {
  // This is our no data state.
  this->last_value = -NAN;
}


void State::eval(){
  for (const auto& binding : this->bindings) {
    binding->eval();
  }
}

void State::reset(){
  for (const auto& binding : this->bindings) {
    binding->reset();
  }
}

std::shared_ptr<Binding> State::addBinding(std::string target_name, std::string input) {
  std::shared_ptr<Binding> binding = std::make_shared<Binding>(target_name, input);
  this->bindings.push_back(binding);

  return binding;
}

void State::removeBinding(std::shared_ptr<Binding> binding) {
  this->bindings.remove(binding);
}

void State::clearBindings() {
  this->bindings.clear();
}


Clockwork::Clockwork(std::string name) {
  this->name = name;
  this->current_state = "default";
}

Clockwork::~Clockwork() {
  // These can't be auto cleaned because there's a global list.
  for(const auto& tag : this->tags) {
    tag->unregister();
  }
}

std::shared_ptr<State> Clockwork::addState(std::string name) {
  std::shared_ptr<State> state = std::make_shared<State>();
  this->states[name] = state;
  return state;
}


void Clockwork::removeState(std::string name) {
  this->states.erase(name);
}

