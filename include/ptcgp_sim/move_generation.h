#pragma once

#include "action.h"
#include "game_state.h"
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// generate_legal_moves
//
// Returns all valid Actions for `player` given the current GameState.
// Rules enforced:
//   - Setup phase: only PlayPokemon (Basic, stage==0) into empty slots + Pass
//   - Action phase: PlayPokemon, AttachEnergy (once/turn, turn>=2), PlaySupporter
//     (once/turn), PlayItem, PlayTool (one per slot), PlayStadium, Retreat
//     (once/turn, if energy covers cost), Attack (if energy covers cost), Pass
// ---------------------------------------------------------------------------
std::vector<Action> generate_legal_moves(const GameState& gs, int player);

// ---------------------------------------------------------------------------
// Energy matching helpers (also used by tests)
// ---------------------------------------------------------------------------

// Returns true if `available` energy satisfies `required` cost.
// Colorless in `required` matches any energy type in `available`.
bool energy_satisfies_cost(const std::vector<EnergyType>& available,
                           const std::vector<EnergyType>& required);

} // namespace ptcgp_sim
