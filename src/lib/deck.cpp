#include "jsmn_helpers.h"

#include "ptcgp_sim/common.h"
#include "ptcgp_sim/deck.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ptcgp_sim 
{

// ---------------------------------------------------------------------------
// Deck::load_from_json
// ---------------------------------------------------------------------------
// Expected JSON format:
// {
//   "energy": ["Psychic", "Colorless"],   // array of one or more energy types
//   "energy": "Psychic",                   // single string also accepted
//   "cards": [
//     { "id": "A4a 064", "count": 2 },
//     { "id": "B1 102",  "count": 2, "name": "Mega Altaria ex" },
//     ...
//   ]
// }

Deck Deck::load_from_json(const std::string& path, const Database& db)
{
    // Read file
    std::ifstream file(path, std::ios::binary);
    assert(file.is_open() && "Deck::load_from_json: cannot open file");

    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string json = ss.str();
    const char* js = json.c_str();
    const int   js_len = (int)json.size();

    // Tokenise
    jsmn_parser parser;
    jsmn_init(&parser);
    int ntok = jsmn_parse(&parser, js, js_len, nullptr, 0);
    assert(ntok >= 0 && "Deck::load_from_json: jsmn failed to parse JSON");

    std::vector<jsmntok_t> tokens((size_t)ntok);
    jsmn_init(&parser);
    jsmn_parse(&parser, js, js_len, tokens.data(), (unsigned int)ntok);

    assert(ntok >= 1 && tokens[0].type == JSMN_OBJECT && "Deck::load_from_json: top-level value must be an object");

    Deck deck;

    int top_size = tokens[0].size;
    int i = 1;

    for (int kv = 0; kv < top_size; ++kv)
    {
        if (i >= ntok) break;
        const jsmntok_t& key = tokens[i];
        ++i;
        if (i >= ntok) break;

        if (jsmn_tok_eq(js, key, "energy"))
        {
            // Accept either a single string or an array of strings
            if (tokens[i].type == JSMN_ARRAY)
            {
                int energy_arr_size = tokens[i].size;
                ++i;
                for (int e = 0; e < energy_arr_size; ++e)
                {
                    if (i >= ntok) break;
                    EnergyType et = energy_from_string(jsmn_tok_str(js, tokens[i]));
                    deck.energy_types.push_back(et);
                    ++i;
                }
            }
            else
            {
                // Single string value
                EnergyType et = energy_from_string(jsmn_tok_str(js, tokens[i]));
                deck.energy_types.push_back(et);
                ++i;
            }
        }
        else if (jsmn_tok_eq(js, key, "cards"))
        {
            assert(tokens[i].type == JSMN_ARRAY && "Deck::load_from_json: \"cards\" must be an array");

            int arr_size = tokens[i].size;
            ++i; // move into array elements

            for (int a = 0; a < arr_size; ++a)
            {
                if (i >= ntok) break;
                assert(tokens[i].type == JSMN_OBJECT && "Deck::load_from_json: each card entry must be an object");

                int entry_size = tokens[i].size;
                ++i;

                DeckEntry entry;
                entry.count = 1;

                for (int f = 0; f < entry_size; ++f)
                {
                    if (i >= ntok) break;
                    const jsmntok_t& fkey = tokens[i]; ++i;
                    if (i >= ntok) break;
                    const jsmntok_t& fval = tokens[i];

                    if (jsmn_tok_eq(js, fkey, "id"))
                    {
                        entry.id = Database::parse_id(jsmn_tok_str(js, fval));
                        ++i;
                    }
                    else if (jsmn_tok_eq(js, fkey, "count"))
                    {
                        entry.count = std::stoi(jsmn_tok_str(js, fval));
                        ++i;
                    }
                    else
                    {
                        // Skip unknown field (name, notes, etc.)
                        if (fval.type == JSMN_OBJECT || fval.type == JSMN_ARRAY)
                        {
                            int end = fval.end;
                            ++i;
                            while (i < ntok && tokens[i].start < end) ++i;
                        }
                        else
                        {
                            ++i;
                        }
                    }
                }

                // Resolve card from database and flatten
                const Card* card = db.find_by_id(entry.id);
                assert(card && "Deck::load_from_json: card not found in database");

                deck.entries.push_back(entry);
                for (int c = 0; c < entry.count; ++c)
                    deck.cards.push_back(*card);
            }
        }
        else
        {
            // Skip unknown top-level key value
            const jsmntok_t& val = tokens[i];
            if (val.type == JSMN_OBJECT || val.type == JSMN_ARRAY)
            {
                int end = val.end;
                ++i;
                while (i < ntok && tokens[i].start < end) ++i;
            }
            else
            {
                ++i;
            }
        }
    }

    return deck;
}

// ---------------------------------------------------------------------------
// Deck::validate
// ---------------------------------------------------------------------------

bool Deck::validate(std::vector<std::string>& out_errors) const
{
    out_errors.clear();

    // Rule 1: exactly 20 cards
    int total = total_cards();
    if (total != 20)
    {
        out_errors.push_back(
            "Deck must contain exactly 20 cards, but has " + std::to_string(total));
    }

    // Rule 2: at least one energy type
    if (energy_types.empty())
        out_errors.push_back("Deck energy type is missing");
    
    return out_errors.empty();
}

bool Deck::is_valid() const
{
    std::vector<std::string> errors;
    return validate(errors);
}

int Deck::total_cards() const
{
    int total = 0;
    for (const auto& e : entries)
        total += e.count;
    return total;
}

// ---------------------------------------------------------------------------
// Deck::shuffle
// ---------------------------------------------------------------------------

void Deck::shuffle(std::mt19937& rng)
{
    std::shuffle(cards.begin(), cards.end(), rng);
}

// ---------------------------------------------------------------------------
// Deck::deal_starting_hand
// ---------------------------------------------------------------------------

static bool is_stage0_pokemon(const Card& c)
{
    return c.type == CardType::Pokemon && c.stage == 0;
}

std::vector<Card> Deck::deal_starting_hand(std::mt19937& rng)
{
    const int HAND_SIZE = 5;
    assert(static_cast<int>(cards.size()) >= HAND_SIZE
           && "deal_starting_hand: deck has fewer than 5 cards");

    // Collect indices of all stage-0 Pokemon in the deck
    std::vector<std::size_t> basic_indices;
    for (std::size_t i = 0; i < cards.size(); ++i)
        if (is_stage0_pokemon(cards[i]))
            basic_indices.push_back(i);

    assert(!basic_indices.empty() && "deal_starting_hand: deck contains no stage-0 Pokemon");

    // 1. Randomly pick one basic Pokemon from the pool — guaranteed hand[0]
    std::uniform_int_distribution<std::size_t> pick(0, basic_indices.size() - 1);
    std::size_t chosen_idx = basic_indices[pick(rng)];
    Card first_card = cards[chosen_idx];

    // Remove the chosen basic from the deck so it isn't drawn again
    cards.erase(cards.begin() + static_cast<std::ptrdiff_t>(chosen_idx));

    // 2. Shuffle the remaining 19 cards and draw 4 from the top
    shuffle(rng);

    std::vector<Card> hand;
    hand.reserve(HAND_SIZE);
    hand.push_back(first_card);
    for (int i = 0; i < HAND_SIZE - 1; ++i)
        hand.push_back(cards[static_cast<std::size_t>(i)]);
    cards.erase(cards.begin(), cards.begin() + (HAND_SIZE - 1));

    return hand;
}

} // namespace ptcgp_sim
