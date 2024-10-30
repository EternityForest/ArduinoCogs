#pragma once
#include <stdint.h>
#include <string>
#include <memory>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @file
 * Utility functions
 */

namespace cogs
{

    //! The GIL
    extern SemaphoreHandle_t mutex;

    //! The main thread. May need to set manually if not just using the default arduino
    //! thread.
    extern TaskHandle_t mainThreadHandle;


    //! Get the GIL
    void lock();

    //! Release the GIL
    void unlock();

    // Wake the main thread, if it exists and is sleeping with waitFrame()
    //! Do not use in ISR
    void wakeMainThread();

    //! Wake main thread from ISR
    void wakeMainThreadISR();

    //! Get the hostname of the device, from /config/device.json
    std::string getHostname();

    //! If a file does not exist or is empty, write the default content.
    void setDefaultFile(const std::string &fn, const std::string &content);
    void logError(const std::string &msg);
    void logInfo(const std::string &msg);

    bool isValidIdentifier(const std::string &str);
    void logErrorIfBadIdentifier(const std::string &str);

    //! Delay till next frame
    void waitFrame();

    /// Random 32 bit int.  Randomness is NOT cryptographically secure.
    /// But should use real entropy from micros timing in many cases.
    uint32_t random(); //flawfinder: ignore

    /// Random in range, min inclusive, max exclusive
    /// random(0, 2) -> 0 or 1
    uint32_t random(uint32_t min, uint32_t max);  //flawfinder: ignore

    /// Like millis() but 64 bits
    uint64_t uptime();

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

    /// blend between two floats
    inline float blend(float a, float b, float t){
        return a * (1.0f - t) + b * t;
    }

    /// Get blend amount between old and new that would make a
    /// first order filter if called at sps hz, for a time constant of tc.
    /// This is based on the formula for an exponential moving average:
    ///   blend = 1 - exp(-dt / tc)
    /// where dt is the time step (1/sps) and tc is the time constant.
    /// Math done by AI
    float getFilterBlendConstant(float tc, float sps);

    /// A faster approximation that doesn't use exp()
    /// I have absolutely no idea how it works
    /// The math was done by AI
    float fastGetApproxFilterBlendConstant(float tc, float sps);

}