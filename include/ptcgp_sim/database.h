#pragma once

#include "card.h"
#include <string>
#include <vector>

// PTCGP_DATABASE_PATH is injected by CMake via target_compile_definitions
#ifndef PTCGP_DATABASE_PATH
#  define PTCGP_DATABASE_PATH "database/database.json"
#endif

namespace ptcgp_sim 
{

class Database 
{
public:
    // Load from the CMake-injected default path
    static Database load();

    // Load from an explicit file path
    static Database load(const std::string& path);

    // Find a card by its full id string, e.g. "A1 002"
    // Returns nullptr if not found.
    const Card* find_by_id(const std::string& id) const;

    const std::vector<Card>& all_cards() const { return Cards; }
    std::size_t              size()      const { return Cards.size(); }

private:
    std::vector<Card> Cards;
};

} // namespace ptcgp_sim
