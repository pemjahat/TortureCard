#include "ptcgp_sim/simulator.h"

namespace ptcgp_sim {

SimulationResult Simulator::run(const Deck& /*deck_p1*/, const Deck& /*deck_p2*/) {
    // TODO: implement full game loop
    return SimulationResult{};
}

void Simulator::run_batch(const Deck& deck_p1, const Deck& deck_p2,
                          int games, int& p1_wins, int& p2_wins) {
    p1_wins = 0;
    p2_wins = 0;
    for (int i = 0; i < games; ++i) {
        auto result = run(deck_p1, deck_p2);
        if (result.winner == 0) ++p1_wins;
        else if (result.winner == 1) ++p2_wins;
    }
}

} // namespace ptcgp_sim
