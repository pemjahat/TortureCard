#pragma once

#include "card.h"
#include "database.h"

#include <vector>

namespace ptcgp_sim 
{

// One entry in a deck JSON: a card id + how many copies.
struct DeckEntry 
{
    CardId id;
    int    count{1};
};

struct Deck 
{
    std::vector<EnergyType>   energy_types; // one or more energy types for this deck
    std::vector<DeckEntry>    entries;      // raw entries (id + count)
    std::vector<Card>      cards;       // flattened cards (count copies each)

    // Load a deck from a JSON file.  Asserts on failure.
    static Deck load_from_json(const std::string& path, const Database& db);

    // Returns true when the deck passes all rules.
    // On failure, fills out_errors with human-readable messages.
    bool validate(std::vector<std::string>& out_errors) const;

    // Convenience: returns true iff validate() produces no errors.
    bool is_valid() const;

    // Total number of cards (sum of all entry counts).
    int total_cards() const;
};

} // namespace ptcgp_sim
