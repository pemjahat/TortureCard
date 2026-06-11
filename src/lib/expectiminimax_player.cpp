#include "ptcgp_sim/expectiminimax_player.h"
#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/move_generation.h"
#include "ptcgp_sim/attack_mechanic.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace ptcgp_sim
{

// ===========================================================================
// Debug tree node types
// ===========================================================================

struct DebugActionNode;

struct DebugStateNode
{
    int    acting_player{0};
    double proba{1.0};
    double value{0.0};
    std::vector<DebugActionNode> children;
};

struct DebugActionNode
{
    Action action;
    double value{0.0};
    std::vector<DebugStateNode> children; // one per stochastic outcome
};

// ===========================================================================
// baseline_value_function
// ===========================================================================

double baseline_value_function(const GameState& gs, int player)
{
    // Terminal states
    if (gs.game_over)
    {
        if (gs.winner == player)  return  1'000'000.0;
        if (gs.winner == -1)      return  0.0;          // draw
        return -1'000'000.0;                             // loss
    }

    const int opponent = (player + 1) % 2;

    auto compute_features = [&](int p) -> double
    {
        const PlayerState& ps = gs.players[p];

        // Feature: prize points scored (weight 10 000)
        double score = ps.points * 10'000.0;

        // Feature: hand size (weight 1)
        score += static_cast<double>(ps.hand.size()) * 1.0;

        // Feature: deck size (weight 1)
        score += static_cast<double>(ps.deck.cards.size()) * 1.0;

        // Active Pokemon features
        const auto& active_slot = ps.pokemon_slots[0];
        if (active_slot.has_value())
        {
            const InPlayPokemon& active = *active_slot;

            // Feature: active HP ratio (weight 500)
            double hp_ratio = static_cast<double>(active.remaining_hp())
                            / static_cast<double>(active.max_hp());
            score += hp_ratio * 500.0;

            // Feature: active can attack (weight 500)
            bool can_attack = false;
            for (const Attack& atk : active.card.attacks)
            {
                if (energy_satisfies_cost(active.attached_energy, atk.energy_required))
                {
                    can_attack = true;
                    break;
                }
            }
            score += (can_attack ? 1.0 : 0.0) * 500.0;
        }

        // Feature: bench count (weight 10)
        int bench_count = 0;
        for (int s = 1; s <= 3; ++s)
            if (ps.pokemon_slots[s].has_value()) ++bench_count;
        score += bench_count * 10.0;

        // Feature: total energy attached to all in-play Pokemon (weight 5)
        int total_energy = 0;
        for (int s = 0; s < 4; ++s)
        {
            if (ps.pokemon_slots[s].has_value())
                total_energy += static_cast<int>(ps.pokemon_slots[s]->attached_energy.size());
        }
        score += total_energy * 5.0;

        return score;
    };

    return compute_features(player) - compute_features(opponent);
}

// ===========================================================================
// Turn-transition helper
// ===========================================================================

// Advance the game state through the end-of-turn sequence after a
// turn-ending action (Attack or Pass) has already been applied.
// Mirrors what GameLoop does:
//   1. Promote any KO'd active Pokemon on both sides
//   2. Apply status damage (Poison / Burn) and promote again
//   3. advance_phase: Cleanup -> Draw (switches current_player, increments turn)
//   4. Draw one card for the new current player (if deck non-empty)
//   5. advance_phase: Draw -> Action
//   6. Generate energy for the new current player (turn >= 2)
//
// After this call, gs.current_player is the opponent and the state is ready
// for the opponent's Action phase — exactly what expectiminimax needs to
// reach a MIN node.
static void advance_turn_in_search(GameState& gs, std::mt19937& rng)
{
    if (gs.game_over) return;

    // 1. Promote KO'd actives (both sides)
    for (int side = 0; side < 2; ++side)
    {
        auto& active = gs.players[side].pokemon_slots[0];
        if (!active.has_value())
        {
            for (int s = 1; s <= 3; ++s)
            {
                auto& bench = gs.players[side].pokemon_slots[s];
                if (bench.has_value())
                {
                    active = std::move(bench);
                    bench  = std::nullopt;
                    active->played_this_turn = false;
                    break;
                }
            }
        }
    }
    if (gs.game_over) return;

    // 2. Status damage (Poison / Burn) on all in-play Pokemon
    for (int p = 0; p < 2; ++p)
    {
        for (int s = 0; s < 4; ++s)
        {
            auto& slot = gs.players[p].pokemon_slots[s];
            if (!slot.has_value()) continue;
            InPlayPokemon& ip = *slot;
            if (ip.status == StatusCondition::Poisoned ||
                ip.status == StatusCondition::Burned)
            {
                int nc = ip.damage_counters + 10;
                ip.damage_counters = std::min(nc, ip.max_hp());
            }
        }
    }
    resolve_knockouts(gs, gs.current_player);
    if (gs.game_over) return;

    // Promote again after status KOs
    for (int side = 0; side < 2; ++side)
    {
        auto& active = gs.players[side].pokemon_slots[0];
        if (!active.has_value())
        {
            for (int s = 1; s <= 3; ++s)
            {
                auto& bench = gs.players[side].pokemon_slots[s];
                if (bench.has_value())
                {
                    active = std::move(bench);
                    bench  = std::nullopt;
                    active->played_this_turn = false;
                    break;
                }
            }
        }
    }
    if (gs.game_over) return;

    // 3. Cleanup -> Draw: switches current_player, increments turn_number,
    //    resets per-turn flags (including played_this_turn on all Pokemon)
    gs.advance_phase(); // Cleanup -> Draw  (also calls reset_turn_flags)

    // 4. Draw one card for the new current player
    {
        const int p = gs.current_player;
        auto& deck = gs.players[p].deck;
        auto& hand = gs.players[p].hand;
        if (!deck.cards.empty())
        {
            hand.push_back(deck.cards.front());
            deck.cards.erase(deck.cards.begin());
        }
    }

    // 5. Draw -> Action
    gs.advance_phase();

    // 6. Generate energy for the new current player (turn >= 2)
    if (gs.turn_number >= 2 && !gs.current_energy.has_value())
    {
        const int p = gs.current_player;
        const auto& energy_types = gs.players[p].deck.energy_types;
        if (!energy_types.empty())
        {
            if (energy_types.size() == 1)
            {
                gs.current_energy = energy_types[0];
            }
            else
            {
                std::uniform_int_distribution<std::size_t> dist(0, energy_types.size() - 1);
                gs.current_energy = energy_types[dist(rng)];
            }
        }
    }
}

// ===========================================================================
// Stochastic outcome enumeration helpers
// ===========================================================================

// Represents one possible outcome of applying an action.
struct Outcome
{
    double    probability{1.0};
    GameState state;
};

// Returns true if the attack at attack_index uses a coin-flip mechanic.
static bool attack_has_coin_flip(const GameState& gs, int player, int attack_index)
{
    const auto& active = gs.players[player].pokemon_slots[0];
    if (!active.has_value()) return false;
    const auto& attacks = active->card.attacks;
    if (attack_index < 0 || attack_index >= static_cast<int>(attacks.size())) return false;
    const AttackMechanic* mech = attacks[attack_index].mechanic.get();
    if (!mech) return false;
    const std::string tn = mech->type_name();
    return tn == "FlipNCoinDamage"
        || tn == "FlipNCoinExtraDamage"
        || tn == "FlipUntilTailsDamage";
}

// Enumerate all stochastic outcomes of applying `action` to `state`.
// For deterministic actions: returns a single outcome with probability 1.0.
// For energy generation: one outcome per energy type in the deck.
// For coin-flip attacks: uses a fixed RNG to sample representative outcomes
//   (heads / tails for single-coin; expected-value approximation for multi-coin).
//
// NOTE: Full enumeration of all 2^N coin outcomes is exponential and
// impractical for N > 1.  Instead we use the expected-value shortcut:
//   - For FlipNCoinDamage / FlipNCoinExtraDamage with N coins, we produce
//     two representative outcomes: all-heads (prob 0.5^N) and all-tails
//     (prob 1 - 0.5^N) as a conservative approximation.
//   - For FlipUntilTailsDamage we use a single expected-value outcome.
//
// This keeps the branching factor bounded while still capturing the
// stochastic nature of coin-flip attacks.
static std::vector<Outcome> enumerate_outcomes(const GameState& state,
                                               const Action& action,
                                               std::mt19937& rng)
{
    std::vector<Outcome> outcomes;

    // -----------------------------------------------------------------------
    // Energy generation: if current_energy is not yet set and the action is
    // not AttachEnergy, we branch over all possible energy types.
    // (Energy is generated at the start of the Action phase, before any
    //  player action.  We model it here as a CHANCE node on the first action
    //  of the turn when current_energy is nullopt and turn >= 2.)
    // -----------------------------------------------------------------------
    // NOTE: Energy generation is handled by GameLoop::generate_energy before
    // run_action_phase, so by the time decide() is called current_energy is
    // already set.  We therefore do NOT branch on energy here — the state
    // passed to decide() already has current_energy resolved.

    // -----------------------------------------------------------------------
    // Coin-flip attacks: enumerate heads/tails outcomes
    // -----------------------------------------------------------------------
    if (action.type == ActionType::Attack)
    {
        const int player = state.current_player;
        if (attack_has_coin_flip(state, player, action.attack_index))
        {
            const auto& active = state.players[player].pokemon_slots[0];
            const AttackMechanic* mech =
                active->card.attacks[action.attack_index].mechanic.get();
            const std::string tn = mech->type_name();

            if (tn == "FlipNCoinDamage" || tn == "FlipNCoinExtraDamage")
            {
                int coins = 1;
                if (tn == "FlipNCoinDamage")
                {
                    const auto* m = dynamic_cast<const FlipNCoinDamage*>(mech);
                    if (m) coins = m->coins;
                }
                else
                {
                    const auto* m = dynamic_cast<const FlipNCoinExtraDamage*>(mech);
                    if (m) coins = m->coins;
                }

                double p_heads = std::pow(0.5, coins);
                double p_tails = 1.0 - p_heads;

                // Outcome 1: apply with current rng
                GameState s1 = state;
                apply_action(s1, action, rng);
                s1.advance_phase(); // Action -> Attack
                s1.advance_phase(); // Attack -> Cleanup
                advance_turn_in_search(s1, rng);
                outcomes.push_back({p_heads, std::move(s1)});

                // Outcome 2: apply with a fresh rng
                std::mt19937 rng2(rng());
                GameState s2 = state;
                apply_action(s2, action, rng2);
                s2.advance_phase(); // Action -> Attack
                s2.advance_phase(); // Attack -> Cleanup
                advance_turn_in_search(s2, rng2);
                outcomes.push_back({p_tails, std::move(s2)});

                return outcomes;
            }
            // FlipUntilTailsDamage: falls through to single outcome below
        }

        // Non-coin-flip attack (or FlipUntilTailsDamage): single outcome,
        // but still advance the turn so the opponent gets to act.
        GameState s = state;
        apply_action(s, action, rng);
        s.advance_phase(); // Action -> Attack
        s.advance_phase(); // Attack -> Cleanup
        advance_turn_in_search(s, rng);
        outcomes.push_back({1.0, std::move(s)});
        return outcomes;
    }

    // -----------------------------------------------------------------------
    // Pass: turn-ending — advance to opponent's Action phase
    // -----------------------------------------------------------------------
    if (action.type == ActionType::Pass)
    {
        // Pass in the Action phase means: Action -> Attack -> Cleanup -> (next turn)
        // advance_phase is called twice by GameLoop (Action->Attack, Attack->Cleanup)
        // before advance_turn_in_search handles Cleanup->Draw->Action.
        GameState s = state;
        s.advance_phase(); // Action -> Attack
        s.advance_phase(); // Attack -> Cleanup
        advance_turn_in_search(s, rng);
        outcomes.push_back({1.0, std::move(s)});
        return outcomes;
    }

    // -----------------------------------------------------------------------
    // All other actions (PlayPokemon, AttachEnergy, Evolve, PlaySupporter,
    // PlayItem, PlayTool, PlayStadium, Retreat, UseAbility):
    // These do NOT end the turn — current_player stays the same.
    // -----------------------------------------------------------------------
    {
        GameState s = state;
        apply_action(s, action, rng);
        outcomes.push_back({1.0, std::move(s)});
    }
    return outcomes;
}

// ===========================================================================
// Forward declarations for recursive search
// ===========================================================================

static std::pair<double, DebugActionNode>
expected_value(const GameState& state, const Action& action,
               int depth, int myself,
               const ValueFunction& vf, std::mt19937& rng);

static std::pair<double, DebugStateNode>
expectiminimax(const GameState& state, int depth, int myself,
               const ValueFunction& vf, std::mt19937& rng);

// ===========================================================================
// expected_value — CHANCE node
// Applies `action` to `state`, enumerates stochastic outcomes, and returns
// the probability-weighted sum of child expectiminimax values.
// ===========================================================================

static std::pair<double, DebugActionNode>
expected_value(const GameState& state, const Action& action,
               int depth, int myself,
               const ValueFunction& vf, std::mt19937& rng)
{
    DebugActionNode node;
    node.action = action;

    std::vector<Outcome> outcomes = enumerate_outcomes(state, action, rng);

    double weighted_sum = 0.0;
    for (auto& outcome : outcomes)
    {
        auto [child_score, child_node] =
            expectiminimax(outcome.state, depth, myself, vf, rng);
        child_node.proba = outcome.probability;
        weighted_sum += outcome.probability * child_score;
        node.children.push_back(std::move(child_node));
    }

    node.value = weighted_sum;
    return {weighted_sum, std::move(node)};
}

// ===========================================================================
// expectiminimax — MAX / MIN node
// ===========================================================================

static std::pair<double, DebugStateNode>
expectiminimax(const GameState& state, int depth, int myself,
               const ValueFunction& vf, std::mt19937& rng)
{
    // Terminal conditions: game over or depth exhausted.
    if (state.game_over || depth == 0)
    {
        double score = vf(state, myself);
        DebugStateNode node;
        node.acting_player = state.current_player;
        node.proba         = 1.0;
        node.value         = score;
        return {score, std::move(node)};
    }

    // Generate legal moves for the current player
    std::vector<Action> actions = generate_legal_moves(state, state.current_player);
    if (actions.empty())
    {
        double score = vf(state, myself);
        DebugStateNode node;
        node.acting_player = state.current_player;
        node.proba         = 1.0;
        node.value         = score;
        return {score, std::move(node)};
    }

    const bool is_max = (state.current_player == myself);
    double best = is_max ? -std::numeric_limits<double>::infinity()
                         :  std::numeric_limits<double>::infinity();

    DebugStateNode state_node;
    state_node.acting_player = state.current_player;
    state_node.proba         = 0.0; // set by parent

    for (const Action& action : actions)
    {
        auto [score, action_node] =
            expected_value(state, action, depth - 1, myself, vf, rng);

        if (is_max)
            best = std::max(best, score);
        else
            best = std::min(best, score);

        state_node.children.push_back(std::move(action_node));
    }

    state_node.value = best;
    return {best, std::move(state_node)};
}

// ===========================================================================
// Debug tree serialization (.dot format)
// ===========================================================================

static void dot_state_node(const DebugStateNode& node, std::ostringstream& dot,
                            int& state_counter, int& action_counter,
                            int current_id, int myself)
{
    const char* color = (node.acting_player == myself) ? "lightgreen" : "lightcoral";
    dot << "    s" << current_id
        << " [label=\"State\\nPlayer:" << node.acting_player
        << "\\nProba:" << node.proba
        << "\\nValue:" << node.value
        << "\", style=filled, fillcolor=" << color << "];\n";

    for (const DebugActionNode& an : node.children)
    {
        ++action_counter;
        int aid = action_counter;

        dot << "    a" << aid
            << " [label=\"" << an.action.to_string()
            << "\\nValue:" << an.value
            << "\", shape=ellipse, style=filled, fillcolor=lightgrey];\n";
        dot << "    s" << current_id << " -> a" << aid << ";\n";

        for (const DebugStateNode& child : an.children)
        {
            ++state_counter;
            int cid = state_counter;
            dot << "    a" << aid << " -> s" << cid << ";\n";
            dot_state_node(child, dot, state_counter, action_counter, cid, myself);
        }
    }
}

static std::string generate_dot(const DebugStateNode& root, int myself)
{
    std::ostringstream dot;
    dot << "digraph GameTree {\n";
    dot << "    rankdir=TB;\n";
    dot << "    node [shape=box];\n";

    int sc = 0, ac = 0;
    dot_state_node(root, dot, sc, ac, 0, myself);

    dot << "}\n";
    return dot.str();
}

// ===========================================================================
// ExpectiMiniMaxPlayer — constructor
// ===========================================================================

ExpectiMiniMaxPlayer::ExpectiMiniMaxPlayer(ValueFunction value_fn,
                                           int max_depth,
                                           bool write_debug_trees)
    : value_fn_(std::move(value_fn))
    , max_depth_(max_depth)
    , write_debug_trees_(write_debug_trees)
{
    assert(max_depth_ >= 1 && "ExpectiMiniMaxPlayer: max_depth must be >= 1");
}

ExpectiMiniMaxPlayer::ExpectiMiniMaxPlayer(int max_depth, bool write_debug_trees)
    : ExpectiMiniMaxPlayer(baseline_value_function, max_depth, write_debug_trees)
{}

// ===========================================================================
// ExpectiMiniMaxPlayer::decide
// ===========================================================================

Action ExpectiMiniMaxPlayer::decide(const GameState& gs,
                                    const std::vector<Action>& legal_moves)
{
    assert(!legal_moves.empty() && "ExpectiMiniMaxPlayer: no legal moves");

    const int myself = gs.current_player;

    // Use a local RNG so the search does not consume the game's RNG state.
    std::mt19937 search_rng(42);

    // Build debug root node
    DebugStateNode root;
    root.acting_player = myself;
    root.proba         = 1.0;

    double best_score = -std::numeric_limits<double>::infinity();
    std::size_t best_idx = 0;

    for (std::size_t i = 0; i < legal_moves.size(); ++i)
    {
        auto [score, action_node] =
            expected_value(gs, legal_moves[i], max_depth_ - 1, myself,
                           value_fn_, search_rng);
        if (score > best_score)
        {
            best_score = score;
            best_idx   = i;
        }
        root.children.push_back(std::move(action_node));
    }

    root.value = best_score;

    if (write_debug_trees_)
        save_debug_tree(root, gs, gs.turn_number, myself);

    return legal_moves[best_idx];
}

// ===========================================================================
// ExpectiMiniMaxPlayer::save_debug_tree
// ===========================================================================

void ExpectiMiniMaxPlayer::save_debug_tree(const DebugStateNode& root,
                                           const GameState& /*root_state*/,
                                           int turn_number,
                                           int player_idx) const
{
    const std::string folder = "expectiminimax_trees";
    std::filesystem::create_directories(folder);

    // Find next available filename
    int counter = 0;
    std::string filename;
    do
    {
        filename = folder + "/expectiminimax_tree_turn"
                 + std::to_string(turn_number)
                 + "_p" + std::to_string(player_idx)
                 + "_" + std::to_string(counter)
                 + ".dot";
        ++counter;
    } while (std::filesystem::exists(filename));

    std::ofstream f(filename);
    if (f.is_open())
        f << generate_dot(root, player_idx);
}

} // namespace ptcgp_sim
