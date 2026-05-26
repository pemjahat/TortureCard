#pragma once

#include "card.h"
#include "game_state.h"

#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// toolitem_effects.h
//
// Dispatch layer for Tool passive triggers and Item card effects.
// Called from effects.cpp after the card is discarded (Item) or attached (Tool).
//
// To add a new Item:
//   1. Add an is_<name>_item() predicate in toolitem_effects.cpp.
//   2. Add an apply_<name>_effect() function in toolitem_effects.cpp.
//   3. Add a branch in apply_item_effect() in toolitem_effects.cpp.
//   4. Update move generation in move_generation.cpp if the item needs a target slot.
//
// To add a new Tool passive:
//   1. Add an is_<name>_tool() predicate in toolitem_effects.cpp.
//   2. Implement the passive logic inside apply_tool_passive().
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// is_potion_item
//
// Returns true if the given CardId corresponds to a Potion card.
// Used by move_generation.cpp to emit per-slot PlayItem actions.
// ---------------------------------------------------------------------------
bool is_potion_item(const CardId& id);

// ---------------------------------------------------------------------------
// effective_hp
//
// Returns the effective maximum HP of an in-play Pokemon, accounting for any
// attached tool that modifies HP (e.g. Giant Cape adds +20).
// Use this instead of card.hp directly for KO detection and remaining_hp().
// ---------------------------------------------------------------------------
int effective_hp(const InPlayPokemon& ip);

// ---------------------------------------------------------------------------
// apply_item_effect
//
// Resolve the effect of an Item card that was just played by `player`.
// The card has already been removed from hand and placed in the discard pile
// by apply_action before this is called.
//
// `card_id`    — identifies which item was played.
// `target_slot`— slot index (0-3) for items that target a specific Pokemon
//                (e.g. Potion).  Pass -1 for items with no target.
// `rng`        — required by effects that involve randomness.
// ---------------------------------------------------------------------------
void apply_item_effect(GameState& gs, int player, const CardId& card_id,
                       int target_slot, std::mt19937& rng);

// ---------------------------------------------------------------------------
// apply_tool_passive
//
// Called from apply_attack_damage after damage is applied to the defender.
// Triggers any passive tool effects on the defending Pokemon (slot 0 of the
// defending player), e.g. Rocky Helmet counterattack.
//
// `attacker_player` — the player who just attacked (index 0 or 1).
// `damage_dealt`    — the final damage applied; if 0, this is a no-op.
//
// Does NOT call resolve_knockouts — the caller is responsible for that.
// ---------------------------------------------------------------------------
void apply_tool_passive(GameState& gs, int attacker_player, int damage_dealt);

} // namespace ptcgp_sim
