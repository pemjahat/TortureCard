#pragma once

#include "card.h"
namespace ptcgp_sim
{

// Convert a string to EnergyType.
// Convert a string to EnergyType. Returns EnergyType::Unknown for unrecognised values.
inline EnergyType energy_from_string(const std::string& s)
{
    if (s == "Grass")     return EnergyType::Grass;
    if (s == "Fire")      return EnergyType::Fire;
    if (s == "Water")     return EnergyType::Water;
    if (s == "Lightning") return EnergyType::Lightning;
    if (s == "Psychic")   return EnergyType::Psychic;
    if (s == "Fighting")  return EnergyType::Fighting;
    if (s == "Darkness")  return EnergyType::Darkness;
    if (s == "Metal")     return EnergyType::Metal;
    if (s == "Dragon")    return EnergyType::Dragon;
    if (s == "Colorless") return EnergyType::Colorless;
    return EnergyType::Unknown;
}

// Convert an EnergyType to its display string.
inline const char* energy_to_string(EnergyType e)
{
    switch (e)
    {
        case EnergyType::Grass:     return "Grass";
        case EnergyType::Fire:      return "Fire";
        case EnergyType::Water:     return "Water";
        case EnergyType::Lightning: return "Lightning";
        case EnergyType::Psychic:   return "Psychic";
        case EnergyType::Fighting:  return "Fighting";
        case EnergyType::Darkness:  return "Darkness";
        case EnergyType::Metal:     return "Metal";
        case EnergyType::Dragon:    return "Dragon";
        case EnergyType::Colorless: return "Colorless";
        default:                    return "Unknown";
    }
}

} // namespace ptcgp_sim
