#pragma once

#include "ability_mechanic.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// ability_mechanic_dictionary
//
// Returns a static map from raw ability effect text (as it appears in
// database.json) to a prototype AbilityMechanic instance.
//
// Add new entries in ability_mechanic_dictionary.cpp to support additional
// ability mechanics.  No other file needs to be modified.
//
// If an effect text is not in the map, callers should treat it as
// UnknownAbilityMechanic (no crash).
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, std::unique_ptr<AbilityMechanic>>&
ability_mechanic_dictionary();

// ---------------------------------------------------------------------------
// pair_ability_mechanic
//
// Returns a static map from CardId::to_string() to a non-owning pointer to
// the prototype AbilityMechanic for that Pokemon's ability.
//
// Returns nullptr for Pokemon with no ability or an unrecognised ability
// effect text (treat as UnknownAbilityMechanic — no crash).
//
// Built lazily from ability_mechanic_dictionary() on first call.
// Requires the Card struct to carry an `ability` field (name + effect text).
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, const AbilityMechanic*>&
pair_ability_mechanic();

} // namespace ptcgp_sim
