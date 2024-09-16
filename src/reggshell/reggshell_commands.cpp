#include "reggshell.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <Arduino.h>

using namespace reggshell;

static void byte_to_hex(unsigned char value, char *buffer)
{
    buffer[0] = "0123456789ABCDEF"[(value >> 4) & 0xF];
    buffer[1] = "0123456789ABCDEF"[value & 0xF];
}

static void addLeadingSlashIfMissing(char *s)
{
    char buf[128];
    if (s[0] != '/')
    {
        strcpy(buf + 1, s);
        buf[0] = '/';
        strcpy(s, buf);
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
    if (c >= 32 && c <= 126)
    {
        return true;
    }
    return false;
}

static File heredocfile;

static void heredoc(Reggshell *rs, MatchState *ms, const char *raw)
{
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

        heredocfile.write((unsigned char *)raw, strlen(raw));
        heredocfile.write((unsigned char *)"\n", 1);
    }

    else
    {

        char buf[128];
        ms->GetCapture(buf, 0);
        addLeadingSlashIfMissing(buf);
        heredocfile = LittleFS.open(buf, "w");
        rs->takeExclusive();
        rs->print("Opened heredoc: ");
        rs->println(buf);
    }
}

static void printSharFile(Reggshell *rs, const char *fn)
{

    bool printAsHex = false;
    char eof[] = "\n---EOF---";
    char *matchPointer = eof;

    char fn2[64];

    char real_fn[64];

    strcpy(real_fn, fn);
    addLeadingSlashIfMissing(real_fn);

    // If the filename starts with a slash, it's an absolute path.
    // We don't want that because it wouldn't make sense on linux.
    if (fn[0] == '/')
    {
        strcpy(fn2, fn);
    }
    else
    {
        strcpy(fn2, fn + 1);
    }

    File f = LittleFS.open(real_fn, "r");
    if (!f)
    {
        rs->print("Can't open file: ");
        rs->println(fn2);
        return;
    }
    while (f.available())
    {
        char c = f.read();
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
    f = LittleFS.open(real_fn, "r");

    char buf[3];
    buf[2] = 0;

    int count = 0;

    rs->println("");
    rs->println("");

    if (printAsHex)
    {
        rs->print("base64 --decode << \"---EOF---\" >");
        rs->println(fn);
        while (f.available())
        {
            char c = f.read();
            byte_to_hex(c, buf);
            rs->print(buf);

            count++;
            if (count > 40)
            {
                rs->println("");
                count = 0;
            }
        }
    }
    else
    {
        buf[1] = 0;
        rs->print("cat << \"---EOF---\" >");
        rs->println(fn);

        while (f.available())
        {
            char c = f.read();
            buf[0] = c;
            rs->print(buf);
        }
    }
    rs->println("");
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


static void ipCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    reggshell->print("IP: ");
    reggshell->println(WiFi.localIP().toString().c_str());
}


static void catCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    char fn[64];
    strcpy(fn, arg1);
    addLeadingSlashIfMissing(fn);

    char buf[2];
    File f = LittleFS.open(fn, "r");
    if (!f)
    {
        reggshell->println("Can't open file: ");
        reggshell->println(arg1);
        return;
    }

    buf[1] = 0;

    while (f.available())
    {
        buf[0] = f.read();
        reggshell->print(buf);
    }
    f.close();
}

static void lsCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
    char fn[64];
    addLeadingSlashIfMissing(fn);

    const char *defaultDir = "/";
    File dir;

    if (arg1[0] != 0)
    {
        dir = LittleFS.open(fn);
    }

    else
    {
        dir = LittleFS.open(defaultDir);
    }
    if (!dir)
    {
        reggshell->println("Can't open dir: ");
        reggshell->println(arg1);
        return;
    }
    if (!dir.isDirectory())
    {
        reggshell->println("Not a dir: ");
        reggshell->println(arg1);
        return;
    }

    reggshell->print("Files in dir: ");
    reggshell->println(arg1);

    File i = dir.openNextFile();

    while (i)
    {
        reggshell->print("  ");
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
    for (auto cmd : this->commands_map)
    {
        this->println(cmd.first.c_str());
        if(cmd.second->help[0] != 0){
            this->println(cmd.second->help);
        }

        this->println("");
    }
}

static void helpCommand(Reggshell *reggshell, const char *arg1, const char *arg2, const char *arg3)
{
   reggshell->help();
}

void Reggshell::addBuiltins()
{
    this->addCommand("cat *<< *\"---EOF---\" *> *(.*)", heredoc);

    this->addSimpleCommand("echo", echoCommand, "Echoes the arguments");
    this->addSimpleCommand("reggshar", sharCommand, "Prints a file in shareable format which can be sent to another device");
    this->addSimpleCommand("cat", catCommand, "Prints the contents of a file");
    this->addSimpleCommand("ls", lsCommand, "ls <dir>Prints the contents of a directory");
    this->addSimpleCommand("ip", ipCommand, "Prints the current network info.");
    this->addSimpleCommand("help", helpCommand, "Prints this help");
}