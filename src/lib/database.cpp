#include "ptcgp_sim/card.h"
#include <string>
#include <vector>
#include <stdexcept>

// PTCGP_DATABASE_PATH is injected by CMake via target_compile_definitions
#ifndef PTCGP_DATABASE_PATH
#  define PTCGP_DATABASE_PATH "database/database.json"
#endif

namespace ptcgp_sim {

// TODO: parse database.json and populate the card registry
std::vector<Card> load_database(const std::string& path = PTCGP_DATABASE_PATH);

std::vector<Card> load_database(const std::string& /*path*/) {
    // Placeholder: return empty registry until JSON parsing is implemented
    return {};
}

} // namespace ptcgp_sim
