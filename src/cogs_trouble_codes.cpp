#include <string>
#include <list>
#include "cogs_trouble_codes.h"


std::map<std::string, bool> cogs::troubleCodeStatus;

static void persistTroubleCode(const std::string& code, bool clear){

}

void cogs::addTroubleCode(const std::string& code,bool persist)
{
    troubleCodeStatus[code] = true;
    if (persist)
    {
        persistTroubleCode(code, false);
    }
}

void cogs::inactivateTroubleCode(const std::string& code)
{
    if(troubleCodeStatus.contains(code))
    {
        troubleCodeStatus[code] = false;
    }
}

void cogs::clearTroubleCode(const std::string& code)
{
    if(troubleCodeStatus.contains(code))
    {
        if(troubleCodeStatus[code]==false)
        {
            troubleCodeStatus.erase(code);
        }
    }
}