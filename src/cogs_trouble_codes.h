#pragma once
#include <string>
#include <map>

namespace cogs{

    extern std::map<std::string, bool> troubleCodeStatus;

    //! Tell the system a trouble code is active
    void addTroubleCode(const std::string& code, bool persist = false);
    
    //! Tell the system that a trouble code is no longer active
    void inactivateTroubleCode(const std::string& code);

    //! Clear a trouble code from RAM and persistent storage
    void clearTroubleCode(const std::string& code);
}

namespace cogs_trouble_codes{

    /// Load trouble codes from persistent storage
    void load();
}