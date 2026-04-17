#pragma once

#include "card.h"
#include <string>
#include <vector>

// PTCGP_DATABASE_PATH is injected by CMake via target_compile_definitions
#ifndef PTCGP_DATABASE_PATH
#  define PTCGP_DATABASE_PATH "database/database.json"
#endif

// Default paths for the two intermediate dictionary files (relative to the
// directory that contains database.json).
#ifndef PTCGP_MECHANIC_DICT_PATH
#  define PTCGP_MECHANIC_DICT_PATH "database/mechanic_dictionary.json"
#endif

#ifndef PTCGP_ATTACK_DICT_PATH
#  define PTCGP_ATTACK_DICT_PATH "database/attack_mechanic_dictionary.json"
#endif

namespace ptcgp_sim 
{

class Database 
{
public:
    // Load from the CMake-injected default path.
    // Automatically builds intermediate dictionary files if absent.
    static Database load();

    // Load from an explicit file path.
    // Automatically builds intermediate dictionary files if absent.
    static Database load(const std::string& path);

    // Find a card by structured CardId.
    // Returns nullptr if not found.
    const Card* find_by_id(const CardId& id) const;

    // Parse a full id string like "A1 002" into a CardId.
    // Throws std::invalid_argument if the format is unrecognised.
    static CardId parse_id(const std::string& full_id);

    const std::vector<Card>& all_cards() const { return Cards; }
    std::size_t              size()      const { return Cards.size(); }

    // ---------------------------------------------------------------------------
    // build_dictionaries
    //
    // Unconditionally (re)builds both intermediate files:
    //   1. mechanic_dictionary.json   — effect text -> Mechanic type + params
    //   2. attack_mechanic_dictionary.json — pokemon_id -> attacks -> mechanic
    //
    // Called by the CLI --build_dictionary flag and automatically by load()
    // when either file is absent.
    //
    // Reports stats to stdout. Returns false if either file could not be written.
    // ---------------------------------------------------------------------------
    static bool build_dictionaries(
        const std::string& db_path          = PTCGP_DATABASE_PATH,
        const std::string& mechanic_path    = PTCGP_MECHANIC_DICT_PATH,
        const std::string& attack_dict_path = PTCGP_ATTACK_DICT_PATH);

private:
    std::vector<Card> Cards;

    // Internal: parse database.json and populate Cards (without mechanic resolution).
    static Database parse_json(const std::string& path);

    // Internal: apply mechanic resolution to all loaded cards using the
    // attack mechanic dictionary (if available) or effect_mechanic_map().
    void resolve_mechanics(const std::string& attack_dict_path);
};

} // namespace ptcgp_sim
