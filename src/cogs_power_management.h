
namespace cogs_pm
{
    // Whether to allow going to sleep before N seconds of uptime.
    // Used to lokc out sleep the first time to prevent accidental boot loop
    extern const bool allowImmediateSleep;
    extern int fps;
    void begin();
}