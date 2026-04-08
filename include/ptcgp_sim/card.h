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

struct Card 
{
    std::string               id;
    std::string               name;
    CardType                  type{CardType::Pokemon};
    int                       hp{0};
    EnergyType                energy_type{EnergyType::Colorless};
    std::optional<EnergyType> weakness;
    std::vector<EnergyType>   retreat_cost;
};

} // namespace ptcgp_sim
