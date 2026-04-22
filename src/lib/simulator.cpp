#include "ptcgp_sim/simulator.h"
#include "ptcgp_sim/attach_attack_player.h"
#include "ptcgp_sim/game_loop.h"

#include <random>

namespace ptcgp_sim 
{

SimulationResult Simulator::run(const Deck& deck_p0, const Deck& deck_p1) 
{
    AttachAttackPlayer player0;
    AttachAttackPlayer player1;

    std::mt19937 rng(std::random_device{}());
    GameLoop loop(&player0, &player1, rng, /*verbose=*/false);

    GameState gs = GameState::make(deck_p0, deck_p1);
    return loop.run(gs);
}

void Simulator::run_batch(const Deck& deck_p1, const Deck& deck_p2,
                          int games, int& p1_wins, int& p2_wins) 
{
    p1_wins = 0;
    p2_wins = 0;
    for (int i = 0; i < games; ++i) 
    {
        auto result = run(deck_p1, deck_p2);
        if (result.winner == 0) ++p1_wins;
        else if (result.winner == 1) ++p2_wins;
    }
}

} // namespace ptcgp_sim
