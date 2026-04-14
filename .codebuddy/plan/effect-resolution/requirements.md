# Requirements Document: Effect Resolution

## Introduction

This feature implements the core **effect resolution engine** for Torture Red (TortureCard), a C++ Pokemon TCG Pocket simulator. The implementation is guided by the reference logic in `deckgym-core` (Rust) and adapted to the existing C++ architecture in `TortureCard`.

The scope covers three sequential layers of effect resolution:

1. **Normal Attack Damage Resolution** — Apply base attack damage to the defending Pokemon, respecting weakness modifiers.
2. **Knockout & Discard Resolution** — When a Pokemon's HP reaches 0, discard it from play, award prize points to the opponent, and trigger bench promotion (or declare a winner if no bench Pokemon remain).
3. **Basic Item: Pokemon Search Tool** — Resolve a specific Item card that searches the deck for a Basic Pokemon, puts it into the player's hand (or does nothing if none found), and shuffles the deck.

The engine is introduced as a new `apply_action` function (and supporting helpers) in `src/lib/effects.cpp`, with a new header `include/ptcgp_sim/effects.h`. The existing `GameState`, `PlayerState`, `InPlayPokemon`, and `Action` structures remain unchanged.

---

## Requirements

### Requirement 1: Normal Attack Damage Resolution

**User Story:** As a game engine, I want to apply the base damage of the active Pokemon's chosen attack to the opponent's active Pokemon, so that HP is correctly reduced each time an attack action is executed.

#### Acceptance Criteria

1. WHEN an `Action` of type `Attack` is applied THEN the system SHALL look up the attack at `action.attack_index` from the current player's active Pokemon's `attacks` list.
2. WHEN the attack has `damage > 0` THEN the system SHALL add that value to the opponent's active Pokemon's `damage_counters`.
3. WHEN the defending Pokemon has a `weakness` that matches the attacking Pokemon's `energy_type` THEN the system SHALL add an additional **+20 damage** to the damage applied (PTCGP weakness rule).
4. WHEN `damage_counters` are applied THEN the system SHALL NOT allow `damage_counters` to exceed the defending Pokemon's `card.hp` (cap at `card.hp` to keep `remaining_hp()` at 0, not negative).
5. WHEN the attack has `damage == 0` THEN the system SHALL apply zero damage (no-op for damage step).
6. WHEN the attack action is applied THEN the system SHALL set `gs.attacked_this_turn = true`.

---

### Requirement 2: Knockout and Discard Resolution

**User Story:** As a game engine, I want to detect when a Pokemon's HP reaches zero after damage is applied, discard it from play, award prize points, and trigger bench promotion or declare a winner, so that the game state correctly reflects knockouts.

#### Acceptance Criteria

1. WHEN damage is applied and a Pokemon's `is_knocked_out()` returns `true` THEN the system SHALL move that Pokemon (and its attached tool, if any) to the owning player's `discard_pile`.
2. WHEN a Pokemon is knocked out THEN the system SHALL award **1 prize point** to the opponent (`opponent.points += 1`). *(Note: EX Pokemon awarding 2 points is out of scope for this iteration.)*
3. WHEN a player's `points >= 3` THEN the system SHALL set `gs.game_over = true` and `gs.winner` to that player's index.
4. WHEN the knocked-out Pokemon was in the **active slot (slot 0)** AND the owning player has at least one bench Pokemon THEN the system SHALL leave the active slot empty (promotion is a separate player decision, out of scope for this iteration — the slot simply becomes `std::nullopt`).
5. WHEN the knocked-out Pokemon was in the **active slot (slot 0)** AND the owning player has **no bench Pokemon** THEN the system SHALL immediately set `gs.game_over = true` and `gs.winner` to the opponent's index.
6. WHEN a Pokemon in a **bench slot** is knocked out THEN the system SHALL discard it and award points without triggering any promotion logic.
7. WHEN both players simultaneously reach `>= 3 points` (e.g., via counterattack) THEN the system SHALL set `gs.game_over = true` and `gs.winner = -1` (tie).
8. WHEN a Pokemon is discarded from play THEN the system SHALL clear its slot to `std::nullopt` in `pokemon_slots`.

---

### Requirement 3: Basic Item — Pokemon Search (Poke Ball / Search Item)

**User Story:** As a player, I want to play a specific Item card that searches my deck for a Basic Pokemon and puts it into my hand, so that I can set up my board more reliably.

#### Acceptance Criteria

1. WHEN an `Action` of type `PlayItem` is applied for a recognized "search basic Pokemon" item card THEN the system SHALL search the current player's deck for all Basic Pokemon (`stage == 0`, `type == CardType::Pokemon`).
2. WHEN at least one Basic Pokemon is found in the deck THEN the system SHALL select one (deterministically for the first implementation — e.g., the first match found) and move it from the deck to the player's `hand`.
3. WHEN no Basic Pokemon is found in the deck THEN the system SHALL add no card to the hand (the search fails gracefully).
4. AFTER the search (whether successful or not) THEN the system SHALL shuffle the player's deck using the provided `std::mt19937` RNG.
5. WHEN the item card is played THEN the system SHALL remove it from the player's `hand` and add it to the player's `discard_pile`.
6. WHEN the item card is played THEN the system SHALL NOT set `gs.supporter_played_this_turn` (Items have no per-turn limit).
7. IF the card ID does not match any implemented item THEN the system SHALL apply a no-op (the card is discarded from hand with no other effect), ensuring unimplemented items do not crash the engine.

---

## Out of Scope (This Iteration)

- EX Pokemon awarding 2 prize points on knockout.
- Bench promotion decision (choosing which bench Pokemon becomes active after a KO).
- Status condition damage (Poison, Burn, Paralysis, Sleep) during Cleanup phase.
- Attack effects beyond base damage (e.g., coin flips, status infliction, bench damage).
- Supporter card resolution.
- Stadium card resolution.
- Retreat resolution.
- Evolution card playing.
