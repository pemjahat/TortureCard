#pragma once

#include "action.h"
#include "game_state.h"

#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// supporter_effects.h
//
// Dispatch layer for Supporter card effects.
// Called from apply_action (effects.cpp) after the card is discarded and
// supporter_played_this_turn is set.
//
// To add a new Supporter:
//   1. Add an is_<name>() predicate in supporter_effects.cpp.
//   2. Add an apply_<name>_effect() function in supporter_effects.cpp.
//   3. Add a branch in apply_supporter_effect() in supporter_effects.cpp.
// ---------------------------------------------------------------------------

// Resolve the effect of a Supporter card that was just played by `player`.
// `action.card_id` identifies the card; `action.slot_index` carries the
// target bench slot for cards where the active player chooses the target
// (e.g. Cyrus).  For cards where the opponent chooses (e.g. Sabrina),
// slot_index is -1 and a PendingResponse is set on the GameState instead.
void apply_supporter_effect(GameState& gs, int player, const Action& action);

// Resolve a ChooseBenchSlot action submitted by the opponent during a
// Sabrina response window.  Executes the bench-swap and clears
// pending_response.
void apply_choose_bench_slot(GameState& gs, const Action& action);

} // namespace ptcgp_sim
