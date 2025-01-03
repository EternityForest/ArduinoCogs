
#pragma once
#include <vector>
#include <map>
#include <string>

#include <Regexp.h>

/**@file
 * This file defines the reggshell namespace.  It provides a fake unix shell with a simple API.
 * 
 * Notably, it allows redirecting a here doc to a file.  This is not a real unix parser, it only works
 * with this exact pattern: "cat << ---EOF--- > filename".
 * 
 * We do this because there is no good standard file language for packing files into a text file.
 * 
 */
namespace reggshell
{
    class Reggshell;

    class ReggshellCommand
    {
    public:
        std::string pattern;
        void (*callback)(Reggshell *, MatchState *, const char * rawline);
    };

    class ReggshellSimpleCommand
    {
    public:
        std::string name;
        const char * help = NULL;
        void (*callback)(Reggshell *,  const char *, const char *, const char *);
    };

    class Reggshell
    {
    private:
        char line_buffer[128];
        unsigned char line_buffer_len;
        std::vector<ReggshellCommand *> commands;
        void (*output_callback)(const char *) = NULL;


        void addBuiltins();

    public:
        std::vector<void (*)(Reggshell *)> statusCallbacks;


        /// Simple commands
        std::map<std::string, ReggshellSimpleCommand *> commands_map;

        std::map<std::string, std::string> documentation;

        /// If parsing in exclusive mode, this command gets every line
        /// no matter what.
        ReggshellCommand *exclusive = NULL;


        /// The command we are currently executing.
        ReggshellCommand *current = NULL;

        bool ignoreNextNewline = false;
        bool escape = false;

        ///  Parse a line.
        void parseLine(char *line);

        /// Parse one char at a time. If \n, pass it to parseLine
        void parseChar(unsigned char c);

        void clearLineBuffer();

        /// Give a command exclusive mode.  
        /// Every line gets passed to the callback until you call releaseExclusive.
        /// MatchState * will be null in this case as you are taking over and getting raw input.
        /// Can only call from a nonsimple command
        void takeExclusive();

        /// Release and return to normal parsing
        void releaseExclusive();


        /// Add a nonsimple command.  Lines matching pattern will be passed to callback
        void addCommand(const std::string & pattern, void (*callback)(Reggshell *, MatchState *, const char * rawline), const char * help = NULL);

        void addSimpleCommand(const std::string & name, void (*callback)(Reggshell*, const char *, const char *, const char *), const char * help = NULL);


        /// Print to the output.
        void print(const char *s);
        void println(const char *s);

        void write(char c){
            char buf[2] = {c, 0};
            this->print(buf);
        }


        /// Print numbers to the output
        void print(float f);
        void println(float f);



        /// Print help
        void help();



        /// Set output callback. Defaults to Serial.println
        void setOutputCallback(void (*callback)(const char *));


        Reggshell(/* args */);

        ~Reggshell();

    };

};