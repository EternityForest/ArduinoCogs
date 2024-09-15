#include "reggshell.h"
#include <Arduino.h>

using namespace reggshell;

namespace reggshell
{

    static void doSimpleCommand(Reggshell *reggshell, MatchState *ms, const char *rawline)
    {
        char name[64];
        char arg1[64];
        char arg2[64];
        char arg3[64];

        ms->GetCapture(name, 0);

        ms->GetCapture(arg1, 1);
        ms->GetCapture(arg2, 2);
        ms->GetCapture(arg3, 3);

        // strip_quotes(arg1);
        // strip_quotes(arg2);
        // strip_quotes(arg3);

        if (reggshell->commands_map.contains(name))
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
        this->addCommand("([%a%d_]+) *([%a%d_\\./]*) *([%a%d_\\./]*) *([%a%d_\\./]*)", doSimpleCommand);
    }

    Reggshell::~Reggshell()
    {
        for (auto cmd : this->commands)
        {
            delete cmd;
        }

        for (auto cmd : this->commands_map)
        {
            delete cmd.second;
        }
    }

    void Reggshell::setOutputCallback(void (*callback)(const char *))
    {
        this->output_callback = callback;
    }

    void Reggshell::println(const char *s)
    {
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

    void Reggshell::addCommand(std::string pattern, void (*callback)(Reggshell *, MatchState *, const char *rawline))
    {
        auto cmd = new ReggshellCommand();
        cmd->pattern = pattern;
        cmd->callback = callback;
        this->commands.push_back(cmd);
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

        this->line_buffer[this->line_buffer_len] = c;
        this->line_buffer_len++;
    }

    void Reggshell::parseLine(char *line)
    {

        if (this->exclusive)
        {
            this->exclusive->callback(this, nullptr, line);
            return;
        }

        for (auto cmd : this->commands)
        {
            int len = strlen(line);

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

    void Reggshell::addSimpleCommand(std::string name, void (*callback)(Reggshell *, const char *, const char *, const char *))
    {
        auto cmd = new ReggshellSimpleCommand();
        cmd->name = name;
        cmd->callback = callback;
        this->commands_map[name] = cmd;
    }

}