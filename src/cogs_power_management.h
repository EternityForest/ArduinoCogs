
namespace cogs_pm
{
    // Whether to allow going to sleep before N seconds of uptime.
    // Used to lokc out sleep the first time to prevent accidental boot loop
    extern const bool allowImmediateSleep;

    // Readonly, set by tagpoint, overridable by boostFPS
    extern int fps;

    /// While nonzero, fps is boosted to at least 48 regardless of current fps.
    /// Meant to be used as a locks counter, add one to it to boost, then subtract
    /// one when you're done boosting.  Only modify under the gil.
    extern int boostFPS;

    void begin();
}