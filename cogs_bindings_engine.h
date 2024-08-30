#pragma once

/**
@file cogs_bindings_engine.h


This file provides a low-code programming model where you can connect
"Tag Points" together in a way that will be familiar to anyone used to Excel.

## Tags

A Tag Point is a 32 bit integer that can be used to represent any kind of value,
such as an input or an output.

## Bindings

A binding is a rule that sets its value to an expression, which may use other tags.
These expressions are compiled from strings, so they may be loaded dynamically.

They use floating point arithmetic, but the actual variables are fixed.

## Fixed Point Math

To represent fractional values within the tag, we standardize on the special value
of 16384 to be our resolution.  If you are storing, say, degrees celcius, you would likely
want to store it as 16384ths of a degree.

This value is available as $res in expressions.



## Special Values in expressions

### $res

This is the special value of 16384.


*/

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
  const int32_t FXP_RES=16384;

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

  public:
    Binding(std::string target_name, std::string input);

    void eval();

    ~Binding();
  };

}