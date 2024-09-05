#pragma once
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
}