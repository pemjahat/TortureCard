#pragma once

#define JSMN_HEADER
#include "jsmn.h"
#include <cstring>
#include <string>

namespace ptcgp_sim
{

// Extract the token's text as a std::string.
inline std::string jsmn_tok_str(const char* json, const jsmntok_t& t)
{
    return std::string(json + t.start, t.end - t.start);
}

// Return true when the token's text equals the null-terminated string s.
inline bool jsmn_tok_eq(const char* json, const jsmntok_t& t, const char* s)
{
    int len = t.end - t.start;
    return (int)strlen(s) == len && strncmp(json + t.start, s, len) == 0;
}

} // namespace ptcgp_sim
