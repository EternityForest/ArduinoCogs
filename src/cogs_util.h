#pragma once
#include <stdint.h>
#include <string>
#include <Arduino.h>

/**
 * @file
 * Utility functions
 */

namespace cogs{

//! If a file does not exist or is empty, write the default content.
void setDefaultFile(std::string fn, std::string content);
void logError(std::string msg);

/// Random 32 bit int.  Randomness is not cryptographically secure.
/// But should use real entropy from micros timing in many cases.
uint32_t random();

/// Random in range, min inclusive, max exclusive
/// random(0, 2) -> 0 or 1
uint32_t random(uint32_t min, uint32_t max);

}