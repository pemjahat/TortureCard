#pragma once

#include "deck.h"
#include "game_state.h"

namespace ptcgp_sim {

struct SimulationResult {
    int winner{-1};   // 0 or 1
    int turns{0};
};

class Simulator {
public:
    Simulator() = default;

    // Run a single game between two decks; returns the result.
    SimulationResult run(const Deck& deck_p1, const Deck& deck_p2);

    // Run multiple games and return win counts [p1_wins, p2_wins].
    void run_batch(const Deck& deck_p1, const Deck& deck_p2,
                   int games, int& p1_wins, int& p2_wins);
};

} // namespace ptcgp_sim
