#include "reggshell.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <Arduino.h>
#include "util/base64.h"
#include <Wire.h>

#include "cogs_rules.h"

using namespace reggshell;

static void addLeadingSlashIfMissing(char *s)
{
    if (s[0] != '/')
    {
        char buf[128];      // flawfinder: ignore
        strcpy(buf + 1, s); // flawfinder: ignore
        buf[0] = '/';
        strcpy(s, buf); // flawfinder: ignore
    }
}

// static void strip_quotes(char *s)
// {
//     if (s[0] == '"')
//     {
//         // Strip quotes
//         int l = strlen(s) - 2;
//         memccpy(s, &s[1], '"', l);
//         s[l] = 0;
//     }
// }

static bool isPrintableAscii(char c)
{
    if (c == '\n')
    {
        return true;
    }

    if (c == '\t')
    {
        return true;
    }

    if (c >= 32 && c <= 126)
    {
        return true;
    }
    return false;
}

static File heredocfile;

static void heredoc(Reggshell *rs, MatchState *ms, const char *raw)
{
    if (strlen(raw) > 127) // flawfinder: ignore
    {                      // flawfinder: ignore
        rs->println("Line too long");
        return;
    }

    if (rs->exclusive)
    {

        if (strcmp(raw, "---EOF---") == 0)
        {
            rs->println("closing heredoc");
            if (heredocfile)
            {
                heredocfile.flush();
                heredocfile.close();
            }
            rs->releaseExclusive();
            return;
        }

        // Line length already checked
        heredocfile.write(reinterpret_cast<const unsigned char *>(raw), strlen(raw)); // flawfinder: ignore
        heredocfile.write(reinterpret_cast<const unsigned char *>("\n"), 1);
    }

    else
    {

        char buf[128]; // flawfinder: ignore
        ms->GetCapture(buf, 0);
        addLeadingSlashIfMissing(buf);

        std::string fn = buf;

        std::string dirname = fn.substr(0, fn.rfind('/'));
        if (!LittleFS.exists(dirname.c_str()))
        {
            LittleFS.mkdir(dirname.c_str());
        }

        // Only trusted users can access this function, safe to ignore
        heredocfile = LittleFS.open(buf, "w"); // flawfinder: ignore
        rs->takeExclusive();
        rs->print("Opened heredoc: ");
        rs->println(buf);
    }
}

static void heredoc_64(Reggshell *rs, MatchState *ms, const char *raw)
{
    if (strlen(raw) > 127) // flawfinder: ignore
    {                      // flawfinder: ignore
        rs->println("Line too long");
        return;
    }

    if (rs->exclusive)
    {

        if (strcmp(raw, "---EOF---") == 0)
        {
            rs->println("closing heredoc");
            if (heredocfile)
            {
                heredocfile.flush();
                heredocfile.close();
            }
            rs->releaseExclusive();
            return;
        }

        std::string d = base64_decode(std::string(raw));

        // Line length already checked
        heredocfile.write(reinterpret_cast<const unsigned char *>(raw), strlen(raw)); // flawfinder: ignore
        heredocfile.write(reinterpret_cast<const unsigned char *>("\n"), 1);
    }

    else
    {

        char buf[128]; // flawfinder: ignore
        ms->GetCapture(buf, 0);
        addLeadingSlashIfMissing(buf);

        std::string fn = buf;

        std::string dirname = fn.substr(0, fn.rfind('/'));
        if (!LittleFS.exists(dirname.c_str()))
        {
            LittleFS.mkdir(dirname.c_str());
        }

        // Only trusted users can access this function, safe to ignore
        heredocfile = LittleFS.open(buf, "w"); // flawfinder: ignore
        rs->takeExclusive();
        rs->print("Opened heredoc: ");
        rs->println(buf);
    }
}

static void printSharFile(Reggshell *rs, const char *fn)
{
    // Filename is already sanity checked, unsafe string functions are fine

    if (strlen(fn) > 64) // flawfinder: ignore
    {                    // flawfinder: ignore
        rs->println("Line too long");
        return;
    }

    bool printAsHex = false;
    const char eof[] = "\n---EOF---";
    const char *matchPointer = eof;

    // We already checked fn length
    char fn2[64]; // flawfinder: ignore

    char real_fn[64]; // flawfinder: ignore

    strcpy(real_fn, fn); // flawfinder: ignore

    addLeadingSlashIfMissing(real_fn);

    // If the filename starts with a slash, it's an absolute path.
    // We don't want that because it wouldn't make sense on linux.
    if (fn[0] == '/')
    {
        strcpy(fn2, fn + 1); // flawfinder: ignore
    }
    else
    {
        strcpy(fn2, fn); // flawfinder: ignore
    }

    // Only trusted users can access this function, safe to ignore
    File f = LittleFS.open(real_fn, "r"); // flawfinder: ignore
    if (!f)
    {
        rs->print("Can't open file: ");
        rs->println(fn2);
        return;
    }
    while (f.available())
    {
        // No buffer boundaries to check? What's flawfinder on about?
        char c = f.read(); // flawfinder: ignore
        if (!isPrintableAscii(c))
        {
            printAsHex = true;
        }
        if (c == '\r')
        {
            printAsHex = true;
        }

        // Look for the EOF sequence, we need to use binary mode if we see it.
        if (matchPointer >= (eof + sizeof(eof)))
        {
            printAsHex = true;
        }
        else
        {
            if (c == *matchPointer)
            {
                matchPointer++;
            }
            else
            {
                matchPointer = eof;
            }
        }
    }
    f.close();

    // Only trusted users can access this function, safe to ignore
    f = LittleFS.open(real_fn, "r"); // flawfinder: ignore

    rs->println("");
    rs->println("");

    bool lastWasNl = false;
    if (printAsHex)
    {
        int p = 0;
        unsigned char buf[48]; // flawfinder: ignore

        rs->print("base64 --decode << \"---EOF---\" >");
        rs->println(fn2);
        while (f.available())
        {
            // Does flawfinder think we are reading into a buffer?
            char c = f.read(); // flawfinder: ignore
            buf[p] = c;
            p++;

            if (p == 48)
            {
                rs->println(base64_encode(buf, 48).c_str());
                p = 0;
            }
        }

        if (p > 0)
        {
            rs->println(base64_encode(buf, 48).c_str());
        }
        rs->println("");
    }
    else
    {
        char buf[2]; // flawfinder: ignore
        buf[1] = 0;
        rs->print("cat << \"---EOF---\" >");
        rs->println(fn2);

        while (f.available())
        {
            char c = f.read(); // flawfinder: ignore
            c = (c == '\n');
            buf[0] = c;
            rs->print(buf);
        }
    }
    if(!lastWasNl){
        rs->println("");
    }
    rs->println("---EOF---");
    f.close();
}

static void sharCommand(Reggshell *rs, const char *arg1, const char *arg2, const char *arg3)
{
    printSharFile(rs, arg1);
}

static void echoCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    reggshell->println(arg1);

    if (arg2[0] != 0)
    {
        reggshell->println(arg2);
    }

    if (arg3[0] != 0)
    {
        reggshell->println(arg3);
    }
}

static void catCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{

    // The command args should already be pre checked for sanity by the parser.
    char fn[128];     // flawfinder: ignore
    strcpy(fn, arg1); // flawfinder: ignore
    addLeadingSlashIfMissing(fn);

    char buf[2];                     // flawfinder: ignore
    File f = LittleFS.open(fn, "r"); // flawfinder: ignore
    if (!f)
    {
        reggshell->println("Can't open file: ");
        reggshell->println(arg1);
        return;
    }

    if (f.isDirectory())
    {
        reggshell->println("Is a directory: ");
        reggshell->println(arg1);
        return;
    }

    buf[1] = 0;

    while (f.available())
    {
        buf[0] = f.read(); // flawfinder: ignore
        reggshell->print(buf);
    }
    f.close();
}

static void lsCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    char fn[128];     // flawfinder: ignore
    strcpy(fn, arg1); // flawfinder: ignore
    addLeadingSlashIfMissing(fn);

    File dir;

    if (arg1[0] != 0)
    {
        dir = LittleFS.open(fn); // flawfinder: ignore
    }

    else
    {
        const char *defaultDir = "/";
        dir = LittleFS.open(defaultDir); // flawfinder: ignore
    }
    if (!dir)
    {
        reggshell->println("Can't open dir: ");
        reggshell->println(fn);
        return;
    }
    if (!dir.isDirectory())
    {
        reggshell->println("Not a dir: ");
        reggshell->println(fn);
        return;
    }

    reggshell->print("Files in dir: ");
    reggshell->println(fn);

    File i = dir.openNextFile();

    while (i)
    {
        reggshell->print("  -");
        if (i.isDirectory())
        {
            reggshell->print("📂");
        }
        else
        {
            reggshell->print("📰");
        }
        reggshell->println(i.name());
        i = dir.openNextFile();
    }

    dir.close();
    reggshell->println("");
}

void Reggshell::help()
{
    this->println("");
    this->println("Commands:");
    for (auto const &cmd : this->commands_map)
    {
        bool needMargin = strchr(cmd.second->help, '\n');

        // Multiline help strings get margins for readability.
        if (needMargin)
        {
            this->println("");
        }
        this->println(cmd.first.c_str());
        if (cmd.second->help[0] != 0)
        {
            this->println(cmd.second->help);
            if (needMargin)
            {
                this->println("");
            }
        }

        this->println("");
    }
}

static void helpCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    reggshell->help();
}

static void statusCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    reggshell->println("");
    reggshell->println("Status:");

    reggshell->print("  IP: ");
    reggshell->println(WiFi.localIP().toString().c_str());

    reggshell->print("  MAC: ");
    reggshell->println(WiFi.macAddress().c_str());

    reggshell->print("  SSID: ");
    reggshell->println(WiFi.SSID().c_str());

    reggshell->print("  RSSI: ");
    reggshell->println(WiFi.RSSI());

    reggshell->print("  Free RAM: ");
    reggshell->print(((float)ESP.getFreeHeap()) / 1024.0);
    reggshell->println(" KB");

    reggshell->print("  Uptime: ");
    reggshell->print(((float)millis()) / 1000.0);
    reggshell->println(" s");

    reggshell->print("  CPU Temperature: ");
    reggshell->print(temperatureRead());
    reggshell->println(" C");

    #if defined(ESP32) || defined(ESP8266)
    reggshell->print("  Wake Reason: ");
      esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
      switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : reggshell->println("RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1 : reggshell->println("RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER : reggshell->println("Timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : reggshell->println("Touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP : reggshell->println("ULP program"); break;
        default : reggshell->println("Not deep sleep"); break;
      }
    #endif

    reggshell->println("");

    for (auto const &cmd : reggshell->statusCallbacks)
    {
        cmd(reggshell);
    }

    reggshell->println("");

#if defined(ESP32)

    // Get cpu frequency
    reggshell->print("CPU Frequency: ");
    reggshell->print(ESP.getCpuFreqMHz());
    reggshell->println(" MHz");
#endif
}

static void scanCommand(reggshell::Reggshell *interpreter, const char *arg1, const char *arg2, const char *arg3)
{
    interpreter->println("Scanning I2C");
    for (int i = 1; i < 127; i++)
    {
        Wire.beginTransmission(i);
        unsigned char e = Wire.endTransmission(true);
        if (e == 0 || e == 4)
        {
            interpreter->println(i);
        }
    }

    interpreter->println("Done");
}
static void deepSleepCommand(reggshell::Reggshell *interpreter, const char *arg1, const char *arg2, const char *arg3)
{
    interpreter->println("Going to deep sleep");
    auto t = cogs_rules::IntTagPoint::getTag("$deepsleep.time", 3600);
    esp_sleep_enable_timer_wakeup(t->value[0] * 1000000);
    esp_deep_sleep_start();
}

static void rmCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    LittleFS.remove(arg1);
}

void Reggshell::addBuiltins()
{

    this->addCommand("cat *<< *\"---EOF---\" *> *(.*)", heredoc);
    this->addCommand("base64 --decode *<< *\"---EOF---\" *> *(.*)", heredoc_64);

    this->addSimpleCommand("echo", echoCommand, "Echoes the arguments");
    this->addSimpleCommand("reggshar", sharCommand, "Prints a file in shareable format which can be sent to another device");
    this->addSimpleCommand("cat", catCommand, "Prints the contents of a file");
    this->addSimpleCommand("ls", lsCommand, "ls <dir>Prints the contents of a directory");
    this->addSimpleCommand("help", helpCommand, "Prints this help");
    this->addSimpleCommand("status", statusCommand, "Prints status info");
    this->addSimpleCommand("i2cdetect", scanCommand, "Scans for I2C devices");
    this->addSimpleCommand("deepsleep", deepSleepCommand, "Go to deep sleep for $deepsleep.time seconds");
    this->addSimpleCommand("rm", rmCommand, "Removes a file");
}
