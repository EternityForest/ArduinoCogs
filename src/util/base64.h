//
//  base64 encoding and decoding with C++.
//  Version: 2.rc.09 (release candidate)
//

#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A

#include <string>
std::string base64_decode(std::string const& s);
std::string base64_encode(unsigned char const*, size_t len);

#endif /* BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A */