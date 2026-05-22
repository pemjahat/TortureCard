#include "ptcgp_sim/supporter_effects.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/action.h"

#include <cassert>
#include <iostream>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Card identity predicates
// ---------------------------------------------------------------------------

// A1 225 = Sabrina (Genetic Apex)
// A1 272 = Sabrina (Genetic Apex reprint)
static bool is_sabrina(const CardId& id)
{
    if (id.expansion == "A1" && id.number == 225) return true;
    if (id.expansion == "A1" && id.number == 272) return true;
    return false;
}

// A2 150 = Cyrus (Space-Time Smackdown)
static bool is_cyrus(const CardId& id)
{
    if (id.expansion == "A2" && id.number == 150) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Per-card effect implementations
// ---------------------------------------------------------------------------

// Sabrina: opponent chooses which bench slot to move to Active.
// Sets PendingResponse so the game loop asks the opponent before continuing.
static void apply_sabrina_effect(GameState& gs, int player)
{
    const int opponent          = (player + 1) % 2;
    gs.pending_response         = PendingResponse::SabrinaChoice;
    gs.pending_response_player  = opponent;
    // The actual swap is deferred to apply_choose_bench_slot().
}

// Cyrus: active player already chose the target slot (action.slot_index).
// Execute the bench-swap immediately.
static void apply_cyrus_effect(GameState& gs, int player, int target_bench_slot)
{
    assert(target_bench_slot >= 1 && target_bench_slot <= 3 &&
           "apply_cyrus_effect: target_bench_slot must be 1-3");

    const int opponent    = (player + 1) % 2;
    auto& active_slot     = gs.players[opponent].pokemon_slots[0];
    auto& bench_slot      = gs.players[opponent].pokemon_slots[target_bench_slot];

    assert(active_slot.has_value() && "apply_cyrus_effect: opponent has no active Pokemon");
    assert(bench_slot.has_value()  && "apply_cyrus_effect: target bench slot is empty");

    // Clear volatile status on the current active (it moves to bench)
    active_slot->clear_volatile_status();

    std::swap(active_slot, bench_slot);
}

// ---------------------------------------------------------------------------
// apply_supporter_effect
// ---------------------------------------------------------------------------

void apply_supporter_effect(GameState& gs, int player, const Action& action)
{
    if (is_sabrina(action.card_id))
    {
        apply_sabrina_effect(gs, player);
    }
    else if (is_cyrus(action.card_id))
    {
        apply_cyrus_effect(gs, player, action.slot_index);
    }
    else
    {
        // TODO: resolve supporter effect (e.g. Misty, Professor's Research, etc.)
        std::cerr << "[WARN] apply_supporter_effect: unimplemented supporter effect for "
                  << action.card_id.to_string() << "\n";
    }
}

// ---------------------------------------------------------------------------
// apply_choose_bench_slot  (opponent response to Sabrina)
// ---------------------------------------------------------------------------

void apply_choose_bench_slot(GameState& gs, const Action& action)
{
    assert(gs.pending_response == PendingResponse::SabrinaChoice &&
           "apply_choose_bench_slot: no pending SabrinaChoice response");
    assert(action.slot_index >= 1 && action.slot_index <= 3 &&
           "apply_choose_bench_slot: slot_index must be 1-3");

    const int responder = gs.pending_response_player;
    auto& active_slot   = gs.players[responder].pokemon_slots[0];
    auto& bench_slot    = gs.players[responder].pokemon_slots[action.slot_index];

    assert(active_slot.has_value() && "apply_choose_bench_slot: responder has no active Pokemon");
    assert(bench_slot.has_value()  && "apply_choose_bench_slot: target bench slot is empty");

    // Clear volatile status on the current active (it moves to bench)
    active_slot->clear_volatile_status();

    std::swap(active_slot, bench_slot);

    // Clear the pending response
    gs.pending_response        = PendingResponse::None;
    gs.pending_response_player = -1;
}

} // namespace ptcgp_sim
