#pragma once

#include "card.h"
#include <string>
#include <vector>

// PTCGP_DATABASE_PATH is injected by CMake via target_compile_definitions
#ifndef PTCGP_DATABASE_PATH
#  define PTCGP_DATABASE_PATH "database/database.json"
#endif

// Default paths for the intermediate dictionary files (relative to the
// directory that contains database.json).
// attack_mechanic_dictionary.json  — effect text -> AttackMechanic type + params
#ifndef PTCGP_ATTACK_MECHANIC_DICT_PATH
#  define PTCGP_ATTACK_MECHANIC_DICT_PATH "database/attack_mechanic_dictionary.json"
#endif

// ability_mechanic_dictionary.json  — ability effect text -> AbilityMechanic type + params
#ifndef PTCGP_ABILITY_MECHANIC_DICT_PATH
#  define PTCGP_ABILITY_MECHANIC_DICT_PATH "database/ability_mechanic_dictionary.json"
#endif

// pair_mechanic.json                — pokemon_id -> { ability, attacks[] } combined pair
#ifndef PTCGP_PAIR_MECHANIC_PATH
#  define PTCGP_PAIR_MECHANIC_PATH "database/pair_mechanic.json"
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
    // build_attack_mechanic_dictionary
    //
    // Builds the attack-side dictionary file:
    //   attack_mechanic_dictionary.json — effect text -> AttackMechanic type + params
    //
    // Returns false if the file could not be written.
    // ---------------------------------------------------------------------------
    static bool build_attack_mechanic_dictionary(
        const std::string& db_path              = PTCGP_DATABASE_PATH,
        const std::string& attack_mechanic_path = PTCGP_ATTACK_MECHANIC_DICT_PATH);

    // ---------------------------------------------------------------------------
    // build_ability_mechanic_dictionary
    //
    // Builds the ability-side dictionary file:
    //   ability_mechanic_dictionary.json — ability effect text -> AbilityMechanic type + params
    //
    // Returns false if the file could not be written.
    // ---------------------------------------------------------------------------
    static bool build_ability_mechanic_dictionary(
        const std::string& db_path               = PTCGP_DATABASE_PATH,
        const std::string& ability_mechanic_path = PTCGP_ABILITY_MECHANIC_DICT_PATH);

    // ---------------------------------------------------------------------------
    // build_pair_mechanic
    //
    // Builds the combined pair file:
    //   pair_mechanic.json — pokemon_id -> { ability, attacks[] }
    //
    // Also populates the pair_attack_mechanic() and pair_ability_mechanic()
    // runtime maps.  Called once during Database::load().
    // Returns false if the file could not be written.
    // ---------------------------------------------------------------------------
    bool build_pair_mechanic(
        const std::string& pair_mechanic_path = PTCGP_PAIR_MECHANIC_PATH);

    // ---------------------------------------------------------------------------
    // build_dictionaries  (convenience wrapper)
    //
    // Calls build_attack_mechanic_dictionary(), build_ability_mechanic_dictionary(),
    // then build_pair_mechanic() on a freshly parsed database.
    // Used by the CLI --build_dictionary flag.
    // Returns false if any step fails.
    // ---------------------------------------------------------------------------
    static bool build_dictionaries(
        const std::string& db_path               = PTCGP_DATABASE_PATH,
        const std::string& attack_mechanic_path  = PTCGP_ATTACK_MECHANIC_DICT_PATH,
        const std::string& ability_mechanic_path = PTCGP_ABILITY_MECHANIC_DICT_PATH,
        const std::string& pair_mechanic_path    = PTCGP_PAIR_MECHANIC_PATH);

private:
    std::vector<Card> Cards;

    // Internal: parse database.json and populate Cards (without mechanic resolution).
    static Database parse_json(const std::string& path);

    // Internal: apply AttackMechanic resolution to all loaded cards using the
    // pair_mechanic.json file (if available) or attack_mechanic_dictionary().
    void resolve_mechanics(const std::string& pair_mechanic_path);
};

} // namespace ptcgp_sim
