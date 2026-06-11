#pragma once

#include "player.h"
#include "action.h"
#include "game_state.h"

#include <functional>
#include <string>
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// ValueFunction — evaluates a GameState from one player's perspective.
// Returns a double where higher values are better for `player`.
// ---------------------------------------------------------------------------
using ValueFunction = std::function<double(const GameState& gs, int player)>;

// ---------------------------------------------------------------------------
// baseline_value_function
//
// Weighted linear combination of game features, modeled after deckgym-core's
// parametric_value_function.  Returns +1e6 for a win, -1e6 for a loss, 0 for
// a draw, and a finite heuristic score for non-terminal states.
// ---------------------------------------------------------------------------
double baseline_value_function(const GameState& gs, int player);

// ---------------------------------------------------------------------------
// Debug tree nodes (forward-declared; defined in .cpp)
// ---------------------------------------------------------------------------
struct DebugStateNode;
struct DebugActionNode;

// ---------------------------------------------------------------------------
// ExpectiMiniMaxPlayer
//
// Implements the expectiminimax algorithm:
//   MAX nodes  — AI player's own decisions (maximize score)
//   MIN nodes  — opponent's decisions (minimize score)
//   CHANCE nodes — stochastic outcomes (energy generation, coin flips)
//                  weighted by probability
//
// State is mutated in place via apply_action and restored after each branch
// (make/unmake pattern) to avoid expensive full-state copies.
// ---------------------------------------------------------------------------
class ExpectiMiniMaxPlayer : public Player
{
public:
    // Construct with explicit value function and search depth.
    ExpectiMiniMaxPlayer(ValueFunction value_fn, int max_depth = 2,
                         bool write_debug_trees = false);

    // Construct with default baseline value function.
    explicit ExpectiMiniMaxPlayer(int max_depth = 2,
                                  bool write_debug_trees = false);

    Action decide(const GameState& gs,
                  const std::vector<Action>& legal_moves) override;

private:
    ValueFunction value_fn_;
    int           max_depth_;
    bool          write_debug_trees_;

    // Save the search tree to a .dot file for visualization.
    void save_debug_tree(const DebugStateNode& root,
                         const GameState& root_state,
                         int turn_number,
                         int player_idx) const;
};

} // namespace ptcgp_sim
