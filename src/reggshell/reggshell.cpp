#include "reggshell.h"
#include <Arduino.h>
#include <stdexcept>

using namespace reggshell;

namespace reggshell
{

    // Must never be called with corrupted strings, does not check.
    static void doSimpleCommand(Reggshell *reggshell, MatchState *ms, const char *rawline)
    {
        if (strlen(rawline) > 64)
        { // flawfinder: ignore
            reggshell->println("TOOLONG");
            return;
        }

        // Safe to ignore because length check
        char name[64]; // flawfinder: ignore
        char arg1[64]; // flawfinder: ignore
        char arg2[64]; // flawfinder: ignore
        char arg3[64]; // flawfinder: ignore

        ms->GetCapture(name, 0);

        ms->GetCapture(arg1, 1);
        ms->GetCapture(arg2, 2);
        ms->GetCapture(arg3, 3);

        // strip_quotes(arg1);
        // strip_quotes(arg2);
        // strip_quotes(arg3);

        reggshell->print(">>> ");
        reggshell->println(rawline);

        if (reggshell->commands_map.count(name)==1)
        {
            reggshell->commands_map[name]->callback(reggshell, arg1, arg2, arg3);
        }
        else
        {
            reggshell->println("Unknown command");
        }
    }

    Reggshell::Reggshell()
    {
        this->line_buffer_len = 0;
        this->exclusive = NULL;

        this->addBuiltins();
    }

    Reggshell::~Reggshell()
    {
        for (auto const & cmd : this->commands)
        {
            delete cmd;
        }

        for (auto const & cmd : this->commands_map)
        {
            delete cmd.second;
        }
    }

    void Reggshell::setOutputCallback(void (*callback)(const char *))
    {
        this->output_callback = callback;
    }

    void Reggshell::print(float n)
    {
        std::string s = std::to_string(n);
        this->print(s.c_str());
    }
    void Reggshell::println(float n)
    {
        std::string s = std::to_string(n);
        this->println(s.c_str());
    }

    void Reggshell::println(const char *s)
    {
        // Defensive programming
        if (!s)
        {
            s = "NULLPTR";
        }

        if (this->output_callback)
        {
            this->output_callback(s);
            this->output_callback("\n");
        }
        else
        {
            Serial.println(s);
        }
    }

    void Reggshell::print(const char *s)
    {
        if (this->output_callback)
        {
            this->output_callback(s);
        }
        else
        {
            Serial.print(s);
        }
    }

    void Reggshell::addCommand(const std::string &pattern, void (*callback)(Reggshell *, MatchState *, const char *rawline), const char *help)
    {
        auto cmd = new ReggshellCommand();
        cmd->pattern = pattern;
        cmd->callback = callback;
        this->commands.push_back(cmd);
        if(help)
        {
            this->documentation[pattern] = help;
        }
    }

    void Reggshell::parseChar(unsigned char c)
    {

        if (c == '\r')
        {
            return;
        }
        if (c == '\n')
        {
            this->line_buffer[this->line_buffer_len] = 0;
            this->parseLine(this->line_buffer);
            this->line_buffer_len = 0;
            return;
        }

        if (this->line_buffer_len > 120)
        {
            this->println("Line too long");
            return;
        }

        this->line_buffer[this->line_buffer_len] = c;
        this->line_buffer_len++;
    }

    void Reggshell::parseLine(char *line)
    {
        try
        {

            if (this->exclusive)
            {
                this->exclusive->callback(this, nullptr, line);
                return;
            }
            else
            {
                this->print(">>> ");
                this->println(line);
            }

            int len = strlen(line); // flawfinder: ignore
            if (len > 256)
            {
                this->println("Line too long");
                return;
            }

            // outside of exclusive, ignore comment only lines.

            bool found = false;
            for (int i = 0; i < len; i++)
            {
                if (line[i] == ' ')
                {
                    continue;
                }
                else if (line[i] == '#')
                {
                    return;
                }
                else
                {
                    found = true;
                    break;
                }
            }

            // All whitespace, ignore line
            if (!found)
            {
                return;
            }

            for (auto const &cmd : this->commands)
            {
                MatchState ms;
                ms.Target(line);

                char result = ms.Match(cmd->pattern.c_str());

                if (result == REGEXP_MATCHED)
                {
                    this->current = cmd;
                    cmd->callback(this, &ms, line);
                    this->current = NULL;
                    return;
                }
            }

            // Check simple commands last
            MatchState ms;
            ms.Target(line);
            char result = ms.Match("([%a%d_]+) *([%a%d_\\./]*) *([%a%d_\\./]*) *(.*)");
            if (result == REGEXP_MATCHED)
            {
                this->current = NULL;
                doSimpleCommand(this, &ms, line);
                return;
            }

        }
        catch (std::exception &e)
        {
            this->println(e.what());
            return;
        }

        this->println("NOSUCHCOMMAND bad line:");
        this->println(line);
    }

    void Reggshell::takeExclusive()
    {
        if (!this->current)
        {
            this->println("No command is current");
            return;
        }

        this->exclusive = this->current;
    }

    void Reggshell::releaseExclusive()
    {
        this->exclusive = NULL;
    }

    void Reggshell::addSimpleCommand(const std::string & name, void (*callback)(Reggshell *, const char *, const char *, const char *), const char *help)
    {
        auto cmd = new ReggshellSimpleCommand();
        cmd->name = name;
        cmd->callback = callback;
        cmd->help = help;
        this->commands_map[name] = cmd;
    }

}