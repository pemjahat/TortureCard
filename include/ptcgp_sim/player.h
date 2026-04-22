#pragma once

#include "action.h"
#include "game_state.h"
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Player — abstract interface for any decision-making agent.
//
// Concrete implementations receive the current GameState and the list of
// legal moves, and return exactly one Action from that list.
// ---------------------------------------------------------------------------
class Player
{
public:
    virtual ~Player() = default;

    // Choose one action from `legal_moves` given the current game state.
    // The returned Action MUST be one of the elements in `legal_moves`.
    virtual Action decide(const GameState& gs,
                          const std::vector<Action>& legal_moves) = 0;
};

} // namespace ptcgp_sim
