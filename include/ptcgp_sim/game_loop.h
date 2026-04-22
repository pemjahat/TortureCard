#pragma once

#include "game_state.h"
#include "player.h"
#include "simulator.h"
#include <memory>
#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// GameLoop
//
// Drives a GameState through all turn phases until the game ends:
//   Setup -> Draw -> Action -> Attack -> Cleanup -> (next player's Draw) ...
//
// Energy generation, KO resolution, active-Pokemon promotion, and status
// damage are all handled automatically.  Player decisions are delegated to
// the two Player instances provided at construction.
//
// Turn limit: 200 turns.  If reached, the game is declared a draw.
// ---------------------------------------------------------------------------
class GameLoop
{
public:
    // `players[0]` controls player 0, `players[1]` controls player 1.
    // `verbose` enables per-action console logging.
    GameLoop(Player* player0, Player* player1,
             std::mt19937& rng, bool verbose = false);

    // Run the game to completion and return the result.
    SimulationResult run(GameState& gs);

private:
    static constexpr int MAX_TURNS = 200;

    Player*      players_[2];
    std::mt19937& rng_;
    bool          verbose_;

    // Phase handlers
    void run_setup_phase(GameState& gs);
    void run_draw_phase(GameState& gs);
    void run_action_phase(GameState& gs);
    void run_cleanup_phase(GameState& gs);

    // Generate energy for the current player's turn (turn >= 2).
    void generate_energy(GameState& gs);

    // Promote the first available bench Pokemon to active for `player`.
    // No-op if active slot is already occupied.
    void promote_bench_to_active(GameState& gs, int player);

    // Apply end-of-turn status damage (Poison / Burn) for all in-play Pokemon.
    void apply_status_damage(GameState& gs);

    // Verbose logging helper.
    void log(const std::string& msg) const;
};

} // namespace ptcgp_sim
