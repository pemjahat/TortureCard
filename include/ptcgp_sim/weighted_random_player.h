#pragma once

#include "player.h"
#include "action.h"
#include "game_state.h"

#include <cstdint>
#include <random>
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// WeightedRandomPlayer
//
// Stochastic baseline strategy modeled after deckgym-core's
// WeightedRandomPlayer. It samples from the provided legal moves using a
// centralized ActionType -> weight mapping.
// ---------------------------------------------------------------------------
class WeightedRandomPlayer : public Player
{
public:
    WeightedRandomPlayer();
    explicit WeightedRandomPlayer(uint32_t seed);

    Action decide(const GameState& gs,
                  const std::vector<Action>& legal_moves) override;

    // Exposed for tests and debugging so the policy is easy to inspect.
    static int action_weight(ActionType type);

private:
    std::mt19937 rng_;
};

} // namespace ptcgp_sim
