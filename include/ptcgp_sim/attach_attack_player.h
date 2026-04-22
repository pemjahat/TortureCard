#pragma once

#include "player.h"
#include "action.h"
#include "game_state.h"
#include "move_generation.h"
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// AttachAttackPlayer
//
// Simple greedy strategy (mirrors deckgym-core's AttachAttackPlayer):
//   1. If the active Pokemon already has enough energy for its cheapest
//      attack, prefer Attack over attaching more energy.
//   2. Otherwise, if AttachEnergy to slot 0 (active) is available, do it.
//   3. Else if an Attack action is available, use it.
//   4. Fallback: return the first legal move.
// ---------------------------------------------------------------------------
class AttachAttackPlayer : public Player
{
public:
    Action decide(const GameState& gs,
                  const std::vector<Action>& legal_moves) override;

private:
    // Returns true if the active Pokemon already satisfies the energy cost
    // of at least one of its attacks.
    static bool active_can_attack(const GameState& gs, int player);
};

} // namespace ptcgp_sim
