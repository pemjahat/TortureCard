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

struct Card 
{
    CardId                    id;
    std::string               name;
    CardType                  type{CardType::Pokemon};
    int                       hp{0};
    EnergyType                energy_type{EnergyType::Colorless};
    std::optional<EnergyType> weakness;
    std::vector<EnergyType>   retreat_cost;
};

} // namespace ptcgp_sim
