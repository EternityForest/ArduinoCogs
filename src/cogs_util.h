#pragma once
#include <stdint.h>
#include <string>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @file
 * Utility functions
 */

namespace cogs
{
    extern SemaphoreHandle_t mutex;

    // Get the GIL
    void lock();

    // Release the GIL
    void unlock();

    //! Get the hostname of the device, from /config/device.json
    std::string getHostname();

    //! If a file does not exist or is empty, write the default content.
    void setDefaultFile(const std::string &fn, const std::string &content);
    void logError(const std::string &msg);
    void logInfo(const std::string &msg);

    // Delay till next frame
    void waitFrame();

    /// Random 32 bit int.  Randomness is NOT cryptographically secure.
    /// But should use real entropy from micros timing in many cases.
    uint32_t random(); //flawfinder: ignore

    /// Random in range, min inclusive, max exclusive
    /// random(0, 2) -> 0 or 1
    uint32_t random(uint32_t min, uint32_t max);  //flawfinder: ignore


    // Print unit testing for the code itself. Used for dev only.
    void runUnitTests();

    /// Create a directory if it does not exist
    void ensureDirExists(const std::string &dir);

    /// Increment that never returns 0 or negative, used when you just need to
    /// Make a value change as a trigger signal.
    int bang(int);

    /// Return a shared pointer to a string. May perform string interning
    /// and give you a string that already exists.
    /// This is not fast, don't use it in performance critical code.
    std::shared_ptr<const std::string> getSharedString(const std::string &str);


}