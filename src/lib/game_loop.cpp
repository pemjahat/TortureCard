#include "ptcgp_sim/game_loop.h"
#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/move_generation.h"
#include "ptcgp_sim/common.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GameLoop::GameLoop(Player* player0, Player* player1,
                   std::mt19937& rng, bool verbose)
    : players_{player0, player1}
    , rng_(rng)
    , verbose_(verbose)
{}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------

SimulationResult GameLoop::run(GameState& gs)
{
    // Deal starting hands if not already done
    if (gs.players[0].hand.empty() && gs.players[1].hand.empty())
        gs.deal_starting_hands(rng_);

    // -----------------------------------------------------------------------
    // SETUP PHASE — both players place their active (and optionally bench)
    // -----------------------------------------------------------------------
    // Player 0 sets up first, then player 1.
    for (int p = 0; p < 2; ++p)
    {
        gs.current_player = p;
        gs.turn_phase     = TurnPhase::Setup;
        run_setup_phase(gs);
        if (gs.game_over) return {gs.winner, gs.turn_number};
    }

    // -----------------------------------------------------------------------
    // MAIN GAME LOOP
    // -----------------------------------------------------------------------
    // Start with player 0's first turn (Draw phase).
    gs.current_player = 0;
    gs.turn_number    = 1;
    gs.turn_phase     = TurnPhase::Draw;
    gs.reset_turn_flags();

    while (!gs.game_over)
    {
        // Turn limit guard
        if (gs.turn_number > MAX_TURNS)
        {
            log("Turn limit reached — declaring draw.");
            gs.game_over = true;
            gs.winner    = -1;
            break;
        }

        switch (gs.turn_phase)
        {
            case TurnPhase::Draw:
                run_draw_phase(gs);
                break;

            case TurnPhase::Action:
                generate_energy(gs);
                run_action_phase(gs);
                break;

            case TurnPhase::Attack:
                // Attack was already applied during Action phase.
                // Just advance to Cleanup.
                gs.advance_phase();
                break;

            case TurnPhase::Cleanup:
                run_cleanup_phase(gs);
                break;

            case TurnPhase::Setup:
                // Should not reach here in the main loop.
                assert(false && "GameLoop: unexpected Setup phase in main loop");
                break;
        }
    }

    return {gs.winner, gs.turn_number};
}

// ---------------------------------------------------------------------------
// run_setup_phase
// ---------------------------------------------------------------------------

void GameLoop::run_setup_phase(GameState& gs)
{
    const int p = gs.current_player;
    log("=== Setup: Player " + std::to_string(p) + " ===");

    while (true)
    {
        std::vector<Action> moves = generate_legal_moves(gs, p);
        if (moves.empty()) break;

        Action chosen = players_[p]->decide(gs, moves);
        log("  P" + std::to_string(p) + " setup: " + chosen.to_string());

        if (chosen.type == ActionType::Pass)
            break;

        apply_action(gs, chosen, rng_);
    }
}

// ---------------------------------------------------------------------------
// run_draw_phase
// ---------------------------------------------------------------------------

void GameLoop::run_draw_phase(GameState& gs)
{
    const int p = gs.current_player;
    auto& deck = gs.players[p].deck;
    auto& hand = gs.players[p].hand;

    if (!deck.cards.empty())
    {
        hand.push_back(deck.cards.front());
        deck.cards.erase(deck.cards.begin());
        log("T" + std::to_string(gs.turn_number) +
            " P" + std::to_string(p) + " draws: " + hand.back().name);
    }
    else
    {
        log("T" + std::to_string(gs.turn_number) +
            " P" + std::to_string(p) + " deck empty — no draw.");
    }

    gs.advance_phase(); // Draw -> Action
}

// ---------------------------------------------------------------------------
// generate_energy
// ---------------------------------------------------------------------------

void GameLoop::generate_energy(GameState& gs)
{
    // Energy generation starts on turn 2 (each player's second turn).
    // Turn 1 = player 0's first turn, turn 2 = player 1's first turn.
    if (gs.turn_number < 2) return;
    if (gs.current_energy.has_value()) return; // already set

    const int p = gs.current_player;
    const auto& energy_types = gs.players[p].deck.energy_types;
    if (energy_types.empty()) return;

    EnergyType generated;
    if (energy_types.size() == 1)
    {
        generated = energy_types[0];
    }
    else
    {
        std::uniform_int_distribution<std::size_t> dist(0, energy_types.size() - 1);
        generated = energy_types[dist(rng_)];
    }

    gs.current_energy = generated;
    log("  Energy generated: " + std::string(energy_to_string(generated)));
}

// ---------------------------------------------------------------------------
// run_action_phase
// ---------------------------------------------------------------------------

void GameLoop::run_action_phase(GameState& gs)
{
    const int p = gs.current_player;
    log("--- T" + std::to_string(gs.turn_number) +
        " P" + std::to_string(p) + " Action ---");

    while (true)
    {
        std::vector<Action> moves = generate_legal_moves(gs, p);
        assert(!moves.empty() && "GameLoop: no legal moves in Action phase");

        Action chosen = players_[p]->decide(gs, moves);
        log("  P" + std::to_string(p) + " -> " + chosen.to_string());

        if (chosen.type == ActionType::Pass)
        {
            gs.advance_phase(); // Action -> Attack
            gs.advance_phase(); // Attack -> Cleanup (no attack declared)
            break;
        }

        if (chosen.type == ActionType::Attack)
        {
            apply_action(gs, chosen, rng_);
            gs.attacked_this_turn = true;

            // Promote any knocked-out active Pokemon immediately
            for (int side = 0; side < 2; ++side)
                promote_bench_to_active(gs, side);

            if (gs.game_over) break;

            gs.advance_phase(); // Action -> Attack
            gs.advance_phase(); // Attack -> Cleanup
            break;
        }

        // All other actions (PlayPokemon, AttachEnergy, Evolve, etc.)
        apply_action(gs, chosen, rng_);

        // After any action, check if a KO happened (e.g. from an ability)
        for (int side = 0; side < 2; ++side)
            promote_bench_to_active(gs, side);

        if (gs.game_over) break;
    }
}

// ---------------------------------------------------------------------------
// run_cleanup_phase
// ---------------------------------------------------------------------------

void GameLoop::run_cleanup_phase(GameState& gs)
{
    log("--- T" + std::to_string(gs.turn_number) + " Cleanup ---");

    apply_status_damage(gs);

    // Promote any knocked-out active Pokemon
    for (int side = 0; side < 2; ++side)
        promote_bench_to_active(gs, side);

    if (!gs.game_over)
        gs.advance_phase(); // Cleanup -> Draw (next player's turn)
}

// ---------------------------------------------------------------------------
// apply_status_damage
// ---------------------------------------------------------------------------

void GameLoop::apply_status_damage(GameState& gs)
{
    // Apply Poison (10 dmg) and Burn (10 dmg) to all in-play Pokemon.
    // Only active Pokemon can be Poisoned/Burned in standard rules, but we
    // iterate all slots for safety.
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
                int new_counters = ip.damage_counters + 10;
                ip.damage_counters = std::min(new_counters, ip.card.hp);
                log("  Status damage: " + ip.card.name + " takes 10 dmg");
            }
        }
    }

    // Resolve any KOs caused by status damage
    resolve_knockouts(gs, gs.current_player);
}

// ---------------------------------------------------------------------------
// promote_bench_to_active
// ---------------------------------------------------------------------------

void GameLoop::promote_bench_to_active(GameState& gs, int player)
{
    auto& active = gs.players[player].pokemon_slots[0];
    if (active.has_value()) return; // active slot is occupied — nothing to do

    // Scan bench slots 1-3 for the first occupied slot
    for (int s = 1; s <= 3; ++s)
    {
        auto& bench = gs.players[player].pokemon_slots[s];
        if (bench.has_value())
        {
            log("  P" + std::to_string(player) +
                " promotes " + bench->card.name + " to active.");
            // Move bench Pokemon to active slot
            active = std::move(bench);
            bench  = std::nullopt;
            // Clear played_this_turn so it can evolve next turn
            active->played_this_turn = false;
            return;
        }
    }
    // No bench Pokemon available — game_over should already be set by
    // resolve_knockouts; nothing more to do here.
}

// ---------------------------------------------------------------------------
// log
// ---------------------------------------------------------------------------

void GameLoop::log(const std::string& msg) const
{
    if (verbose_)
        std::cout << msg << "\n";
}

} // namespace ptcgp_sim
