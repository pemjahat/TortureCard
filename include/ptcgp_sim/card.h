#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ptcgp_sim 
{

enum class CardType 
{
    Pokemon,
    Trainer,
    Energy,
};

// Sub-classification for Trainer cards
enum class TrainerType
{
    Item,       // One-shot effect, no per-turn limit
    Tool,       // Attaches to a Pokemon; one Tool per Pokemon at a time
    Supporter,  // Powerful effect; limited to one per turn
    Stadium,    // Placed in shared zone; replaces any existing Stadium
};

enum class EnergyType 
{
    Grass,
    Fire,
    Water,
    Lightning,
    Psychic,
    Fighting,
    Darkness,
    Metal,
    Dragon,
    Colorless,
    Unknown,
};

// Structured card identifier: expansion code (e.g. "A1", "B2b") + card number (e.g. 1 for "001")
struct CardId 
{
    std::string expansion; // 2-3 character set code, e.g. "A1", "B2b"
    int         number{0}; // card number stored as int, e.g. "001" -> 1

    // Reconstruct the original full id string, e.g. "A1 001"
    std::string to_string() const;

    bool operator==(const CardId& o) const 
    {
        return expansion == o.expansion && number == o.number;
    }
    bool operator!=(const CardId& o) const { return !(*this == o); }
};

// A single attack on a Pokemon card
struct Attack
{
    std::string            name;
    std::vector<EnergyType> energy_required; // energy cost to use this attack
    int                    damage{0};        // base damage dealt
};

struct Card 
{
    CardId                    id;
    std::string               name;
    CardType                  type{CardType::Pokemon};

    // --- Pokemon fields ---
    int                       hp{0};
    EnergyType                energy_type{EnergyType::Colorless};
    std::optional<EnergyType> weakness;
    std::vector<EnergyType>   retreat_cost;
    int                       stage{0};       // 0 = Basic, 1 = Stage 1, 2 = Stage 2
    std::vector<Attack>       attacks;

    // --- Trainer fields ---
    TrainerType               trainer_type{TrainerType::Item}; // meaningful only when type == Trainer
};

} // namespace ptcgp_sim
