#pragma once

#include "card.h"
#include "database.h"

#include <random>
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
    std::vector<Card>         cards;        // flattened cards (count copies each)

    // Load a deck from a JSON file.  Asserts on failure.
    static Deck load_from_json(const std::string& path, const Database& db);

    // Returns true when the deck passes all rules.
    // On failure, fills out_errors with human-readable messages.
    bool validate(std::vector<std::string>& out_errors) const;

    // Convenience: returns true iff validate() produces no errors.
    bool is_valid() const;

    // Total number of cards (sum of all entry counts).
    int total_cards() const;

    // Randomly reorder all cards in `cards` using the provided RNG.
    // Does not alter energy_types or entries.
    void shuffle(std::mt19937& rng);

    // Draw a valid starting hand of exactly 5 cards from the top of the deck.
    // Guarantees at least one stage-0 Pokemon in the returned hand.
    // If the initial 5 cards contain no stage-0 Pokemon, the cards are returned
    // to the deck, the deck is reshuffled, and 5 cards are drawn again.
    // The first card in the returned vector is always a stage-0 Pokemon.
    // Asserts if the deck contains no stage-0 Pokemon at all.
    // Removes the 5 dealt cards from `cards`.
    std::vector<Card> deal_starting_hand(std::mt19937& rng);
};

} // namespace ptcgp_sim
