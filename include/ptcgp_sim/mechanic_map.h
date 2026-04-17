#pragma once

#include "mechanic.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// effect_mechanic_map
//
// Returns a static map from raw attack effect text (as it appears in
// database.json) to a prototype Mechanic instance.
//
// Add new entries in mechanic_map.cpp to support additional mechanics.
// No other file needs to be modified.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, std::unique_ptr<Mechanic>>& effect_mechanic_map();

} // namespace ptcgp_sim
