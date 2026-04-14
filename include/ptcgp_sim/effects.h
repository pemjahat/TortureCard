#pragma once

#include "action.h"
#include "game_state.h"

#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// apply_action
//
// Applies a single Action to the GameState, mutating it in place.
// Handles:
//   - Attack:     base damage + weakness modifier + knockout resolution
//   - PlayItem:   implemented item effects (e.g. Poke Ball basic search)
//   - PlayTool:   attaches the tool card to the target slot
//   - PlayPokemon, AttachEnergy, Retreat, PlaySupporter, PlayStadium, Pass:
//                 basic state mutations (no effect resolution yet)
//
// `rng` is used for deck shuffles (e.g. after a search item).
// ---------------------------------------------------------------------------
void apply_action(GameState& gs, const Action& action, std::mt19937& rng);

// ---------------------------------------------------------------------------
// Internal helpers (exposed for unit testing)
// ---------------------------------------------------------------------------

// Apply base attack damage (with weakness) from `attacker_player`'s active
// Pokemon to `defender_player`'s active Pokemon.
// Returns the final damage applied (after weakness, before cap).
int apply_attack_damage(GameState& gs, int attacker_player, int attack_index);

// Scan all in-play Pokemon for knockouts, discard them, award points,
// and update game_over / winner.  Called after any damage step.
void resolve_knockouts(GameState& gs, int attacking_player);

// Search `player`'s deck for the first Basic Pokemon, move it to hand,
// then shuffle the deck.  Returns true if a card was found.
bool search_basic_to_hand(GameState& gs, int player, std::mt19937& rng);

} // namespace ptcgp_sim
