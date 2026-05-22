# Requirements Document: Sabrina & Cyrus Supporter Effects

## Introduction

This feature implements the runtime effects for two Supporter cards — **Sabrina** and **Cyrus** — both of which force the opponent to swap one of their benched Pokémon into the Active slot.

The implementation covers four areas:

1. **Priority-passing state** in `game_state.h`: a new `pending_response` field that records when the active player has played Sabrina and is waiting for the opponent to choose a bench slot.
2. **New action type** `ChooseBenchSlot`: the only legal action available to the opponent while they hold priority during a Sabrina response window.
3. **Move-generation legality** in `move_generation.cpp`: emit correct actions for both the active player (playing Sabrina/Cyrus) and the opponent (responding to Sabrina).
4. **Effect resolution** in `effects.cpp` and **game loop** in `game_loop.cpp`: execute the bench-swap after the opponent's choice is received, then return priority to the active player.

### Card Identities (from deck files)

| Card    | Expansion | Number | Condition to play | Target chooser |
|---------|-----------|--------|-------------------|----------------|
| Sabrina | A1        | 225    | Opponent has ≥ 1 Pokémon on the bench | **Opponent** chooses which bench slot |
| Sabrina | A1        | 272    | Same as above (reprint) | **Opponent** chooses which bench slot |
| Cyrus   | A2        | 150    | Opponent has ≥ 1 **damaged** bench Pokémon (`damage_counters > 0`) | **Active player** chooses which damaged bench slot |

### Effect Summary

Both cards share the same mechanical outcome: **the chosen opponent bench Pokémon is moved to the opponent's Active slot, and the previous Active Pokémon is moved to the chosen bench slot** (a direct swap). The previously-Active Pokémon's volatile status conditions are cleared on the swap (matching the existing `Retreat` behaviour).

The key difference is **who chooses the target**:
- **Sabrina**: the *opponent* (the player being affected) chooses which of their own bench Pokémon to move forward. This requires temporarily passing priority to the opponent.
- **Cyrus**: the *active player* (the one playing the card) chooses which damaged bench Pokémon to pull forward. No priority-passing is needed.

---

## Requirements

### Requirement 1 — Priority-Passing State: `pending_response`

**User Story:** As the game engine, I need a way to represent that the active player has played Sabrina and is waiting for the opponent to make a choice, so the game loop can correctly ask the opponent to decide before continuing.

#### Acceptance Criteria

1. WHEN Sabrina is played THEN `GameState` SHALL set a `pending_response` field to `PendingResponse::SabrinaChoice` (a new enum value), recording that the opponent must choose a bench slot before the effect resolves.
2. WHEN `pending_response` is `SabrinaChoice` THEN `GameState` SHALL also store `pending_response_player` (the index of the player who must respond, i.e. the opponent).
3. WHEN the opponent submits a `ChooseBenchSlot` action THEN `pending_response` SHALL be cleared back to `PendingResponse::None`.
4. WHEN `reset_turn_flags()` is called THEN `pending_response` SHALL be reset to `PendingResponse::None` (safety guard for turn transitions).

---

### Requirement 2 — New Action Type: `ChooseBenchSlot`

**User Story:** As the game engine, I need a dedicated action type for the opponent's bench-slot selection during a Sabrina response window, so it is clearly distinct from a normal `Retreat` or `PlaySupporter` action.

#### Acceptance Criteria

1. WHEN the opponent holds priority during a Sabrina response THEN `ActionType::ChooseBenchSlot` SHALL be a valid action type in the `ActionType` enum.
2. WHEN a `ChooseBenchSlot` action is constructed THEN it SHALL carry a `slot_index` (1–3) identifying which of the opponent's own bench slots to move to Active.
3. WHEN `Action::to_string()` is called on a `ChooseBenchSlot` action THEN the string SHALL include the slot index for debug readability.

---

### Requirement 3 — Action Data: `slot_index` for `PlaySupporter` (Cyrus)

**User Story:** As the game engine, I need the `PlaySupporter` action to carry a target bench slot index for Cyrus, so the active player can specify which damaged opponent bench Pokémon to pull forward.

#### Acceptance Criteria

1. WHEN a `PlaySupporter` action is constructed for Cyrus THEN the `Action` struct SHALL store a `slot_index` field (opponent bench slot 1–3) alongside the existing `card_id`.
2. WHEN `Action::play_supporter(id)` is called without a slot THEN `slot_index` SHALL default to `-1` (no target required, for non-targeting supporters such as Sabrina which uses the separate `ChooseBenchSlot` flow).
3. WHEN `Action::play_supporter(id, slot)` is called with a slot index THEN `slot_index` SHALL be set to that value.
4. WHEN `Action::to_string()` is called on a `PlaySupporter` action with `slot_index != -1` THEN the string SHALL include the slot index for debug readability.

---

### Requirement 4 — Move Generation: Sabrina Legality (Active Player)

**User Story:** As a player, I want `generate_legal_moves` to emit a single `PlaySupporter` action for Sabrina (no slot index) when Sabrina is in my hand and the opponent has at least one bench Pokémon, so I can play the card and trigger the priority-passing window.

#### Acceptance Criteria

1. WHEN Sabrina (`A1 225` or `A1 272`) is in the current player's hand AND `supporter_played_this_turn` is `false` AND the opponent has at least one occupied bench slot (slots 1–3) THEN `generate_legal_moves` SHALL emit exactly one `PlaySupporter(sabrina_id)` action (no slot index; the opponent will choose later).
2. WHEN Sabrina is in hand AND the opponent's bench is completely empty (slots 1–3 all `nullopt`) THEN `generate_legal_moves` SHALL NOT emit any `PlaySupporter` action for Sabrina.
3. WHEN `supporter_played_this_turn` is `true` THEN `generate_legal_moves` SHALL NOT emit any `PlaySupporter` action for Sabrina.

---

### Requirement 5 — Move Generation: Sabrina Response (Opponent)

**User Story:** As the opponent, when Sabrina has been played against me, I want `generate_legal_moves` to offer me one `ChooseBenchSlot` action per occupied bench slot, so I can decide which of my Pokémon to move to the Active position.

#### Acceptance Criteria

1. WHEN `pending_response` is `SabrinaChoice` AND it is the opponent's response turn THEN `generate_legal_moves` SHALL emit one `ChooseBenchSlot(slot)` action for each of the opponent's occupied bench slots (slots 1–3).
2. WHEN `pending_response` is `SabrinaChoice` THEN `generate_legal_moves` SHALL NOT emit any other action types (no Pass, no PlayPokemon, no AttachEnergy, etc.) — the only legal actions are `ChooseBenchSlot` options.
3. WHEN the opponent has exactly one bench Pokémon THEN `generate_legal_moves` SHALL emit exactly one `ChooseBenchSlot` action.
4. WHEN the opponent has multiple bench Pokémon THEN `generate_legal_moves` SHALL emit one `ChooseBenchSlot` action per occupied slot.

---

### Requirement 6 — Move Generation: Cyrus Legality (Active Player)

**User Story:** As a player, I want `generate_legal_moves` to emit `PlaySupporter` actions for Cyrus only when the opponent has at least one damaged bench Pokémon, with one action per valid target slot.

#### Acceptance Criteria

1. WHEN Cyrus (`A2 150`) is in the current player's hand AND `supporter_played_this_turn` is `false` AND the opponent has at least one bench Pokémon with `damage_counters > 0` THEN `generate_legal_moves` SHALL emit one `PlaySupporter(cyrus_id, slot)` action for each such damaged bench slot.
2. WHEN Cyrus is in hand AND no opponent bench Pokémon has any damage counters THEN `generate_legal_moves` SHALL NOT emit any `PlaySupporter` action for Cyrus.
3. WHEN Cyrus is in hand AND the opponent's bench is completely empty THEN `generate_legal_moves` SHALL NOT emit any `PlaySupporter` action for Cyrus.
4. WHEN `supporter_played_this_turn` is `true` THEN `generate_legal_moves` SHALL NOT emit any `PlaySupporter` action for Cyrus.

---

### Requirement 7 — Game Loop: Priority-Passing for Sabrina

**User Story:** As the game engine, I want `run_action_phase` to temporarily pass priority to the opponent when Sabrina is played, ask the opponent to choose a bench slot, then return priority to the active player to continue their turn.

#### Acceptance Criteria

1. WHEN `apply_action` processes a `PlaySupporter` for Sabrina THEN `pending_response` SHALL be set to `SabrinaChoice` and `pending_response_player` SHALL be set to the opponent's index, before returning to the game loop.
2. WHEN `run_action_phase` detects `pending_response == SabrinaChoice` after applying a Sabrina action THEN it SHALL call `players_[opponent]->decide(gs, moves)` to obtain the opponent's `ChooseBenchSlot` action.
3. WHEN the opponent's `ChooseBenchSlot` action is received THEN `run_action_phase` SHALL call `apply_action` with that action to execute the bench-swap.
4. WHEN the bench-swap is complete THEN `pending_response` SHALL be cleared to `None` and the action-phase loop SHALL continue with the original active player holding priority.
5. WHEN `pending_response` is `SabrinaChoice` THEN the active player's action loop SHALL be paused — no further active-player moves are generated until the opponent has responded.

---

### Requirement 8 — Effect Resolution: Bench-Swap

**User Story:** As the game engine, I want `apply_action` to correctly execute the bench-swap for both Sabrina (via `ChooseBenchSlot`) and Cyrus (via `PlaySupporter` with slot), so the opponent's chosen bench Pokémon becomes the new Active Pokémon.

#### Acceptance Criteria

1. WHEN `apply_action` processes a `ChooseBenchSlot` action THEN the system SHALL swap `opponent.pokemon_slots[0]` (Active) with `opponent.pokemon_slots[action.slot_index]` (target bench slot).
2. WHEN `apply_action` processes a `PlaySupporter` for Cyrus THEN the system SHALL swap `opponent.pokemon_slots[0]` with `opponent.pokemon_slots[action.slot_index]`.
3. WHEN the swap occurs THEN the previously Active Pokémon's volatile status conditions (Paralyzed, Confused, Asleep, Burned) SHALL be cleared via `clear_volatile_status()`, matching the existing Retreat behaviour.
4. WHEN the swap occurs THEN all attached energy, attached tool, damage counters, and `cards_behind` on both Pokémon SHALL be preserved (only the slot positions change).
5. WHEN `apply_action` processes a `PlaySupporter` for Sabrina THEN `supporter_played_this_turn` SHALL be set to `true` and the Sabrina card SHALL be moved to the discard pile (the swap itself is deferred to the `ChooseBenchSlot` step).
6. WHEN `apply_action` processes a `PlaySupporter` for Cyrus THEN `supporter_played_this_turn` SHALL be set to `true` and the Cyrus card SHALL be moved to the discard pile.
7. WHEN `apply_action` processes a `PlaySupporter` for any card that is NOT Sabrina or Cyrus THEN the existing stub behaviour (discard + set flag + `// TODO` comment) SHALL remain unchanged.

---

### Requirement 9 — Supporter Identity Helpers

**User Story:** As a developer, I want clean helper predicates in `effects.cpp` to identify Sabrina and Cyrus by `CardId`, following the same pattern as `is_search_basic_item`, so future supporters can be added consistently.

#### Acceptance Criteria

1. WHEN a `CardId` matches `A1 225` or `A1 272` THEN `is_sabrina(id)` SHALL return `true`.
2. WHEN a `CardId` matches `A2 150` THEN `is_cyrus(id)` SHALL return `true`.
3. WHEN a `CardId` does not match any known Sabrina or Cyrus ID THEN the respective helper SHALL return `false`.

---

### Requirement 10 — Unit Tests

**User Story:** As a developer, I want unit tests covering the priority-passing flow, move-generation rules, and effect resolution for Sabrina and Cyrus, so regressions are caught automatically.

#### Acceptance Criteria

1. WHEN Sabrina is played THEN the test SHALL verify `pending_response == SabrinaChoice` is set and `supporter_played_this_turn` is `true`.
2. WHEN `pending_response == SabrinaChoice` THEN `generate_legal_moves` for the opponent SHALL return only `ChooseBenchSlot` actions (one per occupied bench slot).
3. WHEN `pending_response == SabrinaChoice` THEN `generate_legal_moves` for the active player SHALL return no actions (priority is with the opponent).
4. WHEN the opponent submits a `ChooseBenchSlot` action THEN the test SHALL verify the bench and active slots are swapped correctly and `pending_response` is cleared.
5. WHEN Sabrina is in hand and opponent bench is empty THEN the test SHALL verify no `PlaySupporter` action is generated for Sabrina.
6. WHEN Cyrus is in hand and opponent has 1 damaged bench Pokémon THEN the test SHALL verify exactly 1 `PlaySupporter` action is generated targeting that slot.
7. WHEN Cyrus is in hand and opponent bench Pokémon have 0 damage counters THEN the test SHALL verify no `PlaySupporter` action is generated for Cyrus.
8. WHEN `apply_action` is called with a Cyrus `PlaySupporter` action THEN the test SHALL verify the swap occurs and the previously Active Pokémon's volatile status is cleared.
9. WHEN the swap occurs (either Sabrina or Cyrus) THEN the test SHALL verify attached energy, tools, and damage counters are preserved on both Pokémon.
