#pragma once

#include "attack_mechanic.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// attack_mechanic_dictionary
//
// Returns a static map from raw attack effect text (as it appears in
// database.json) to a prototype AttackMechanic instance.
//
// Add new entries in attack_mechanic_dictionary.cpp to support additional
// mechanics.  No other file needs to be modified.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, std::unique_ptr<AttackMechanic>>&
attack_mechanic_dictionary();

// ---------------------------------------------------------------------------
// pair_attack_mechanic
//
// Returns a static map from composite key "<CardId::to_string()>:<attack_index>"
// to a non-owning pointer to the prototype AttackMechanic for that attack.
//
// Returns nullptr for attacks with no known mechanic (BasicDamage fallback).
// Built lazily from attack_mechanic_dictionary() on first call.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, const AttackMechanic*>&
pair_attack_mechanic();

} // namespace ptcgp_sim
