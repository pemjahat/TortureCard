# Requirements Document: Pokemon Evolution

## Introduction

This feature adds **Pokemon evolution** to the TortureCard (ptcgp_sim) C++ simulator, modelled after the rules implemented in the `deckgym-core` Rust reference engine. Evolution allows a player to play a Stage 1 or Stage 2 Pokemon card from their hand on top of a compatible in-play Pokemon, replacing it while preserving attached energy, tools, and accumulated damage. The full evolution chain (all lower-stage cards) is tracked inside `InPlayPokemon` so that when an evolved Pokemon is knocked out, every card in the chain is sent to the discard pile together.

Key design references from `deckgym-core`:
- `PokemonCard::evolves_from` — name of the Pokemon this card evolves from.
- `PlayedCard::cards_behind` — ordered list of lower-stage cards stacked under the current top card.
- `apply_evolve` — mutates state: replaces the slot, preserves energy/tool/damage, builds `cards_behind`.
- `discard_from_play` — on KO, discards `cards_behind` + top card + tool to the owner's discard pile.
- Move generation rules: no evolution on the player's first turn, no evolution on a Pokemon that was placed this same turn, stage must increase by exactly 1 (Basic → Stage 1 → Stage 2).

---

## Requirements

### Requirement 1 — Card Data: `evolves_from` Field

**User Story:** As a simulator developer, I want each Pokemon card to carry the name of the Pokemon it evolves from, so that the engine can validate legal evolution targets without hard-coding card IDs.

#### Acceptance Criteria

1. WHEN a `Card` is constructed for a Stage 1 or Stage 2 Pokemon THEN `Card` SHALL contain an `std::optional<std::string> evolves_from` field holding the exact name of the pre-evolution Pokemon (e.g. `"Bulbasaur"` for Ivysaur).
2. WHEN a `Card` is constructed for a Basic (stage 0) Pokemon THEN `Card.evolves_from` SHALL be `std::nullopt`.
3. WHEN a `Card` is loaded from a JSON deck file THEN the loader SHALL populate `evolves_from` from the `"evolves_from"` JSON key if present, otherwise leave it as `std::nullopt`.

---

### Requirement 2 — In-Play State: Evolution Chain Tracking

**User Story:** As a simulator developer, I want `InPlayPokemon` to track all lower-stage cards stacked beneath the current top card, so that the full evolution chain can be discarded correctly on knockout.

#### Acceptance Criteria

1. WHEN `InPlayPokemon` is created for a freshly placed Basic Pokemon THEN `InPlayPokemon.cards_behind` SHALL be an empty `std::vector<Card>`.
2. WHEN a Pokemon evolves THEN `InPlayPokemon.cards_behind` SHALL contain all previously stacked cards in order from bottom (oldest) to top (most recent pre-evolution), with the new evolution card becoming the new `InPlayPokemon.card`.
3. WHEN an evolved Pokemon is knocked out THEN `resolve_knockouts` SHALL move every card in `cards_behind` plus the top `card` (and any attached tool) to the owner's `discard_pile`.
4. WHEN an evolved Pokemon retreats or is moved to the bench THEN `cards_behind` SHALL remain intact (the chain is not broken by retreating).

---

### Requirement 3 — New Action Type: `Evolve`

**User Story:** As a simulator developer, I want an `Evolve` action type so that the engine can represent and apply a player's decision to evolve a specific in-play Pokemon.

#### Acceptance Criteria

1. WHEN `ActionType::Evolve` is added THEN `Action` SHALL carry `card_id` (the evolution card to play from hand) and `slot_index` (the 0-3 slot of the Pokemon to evolve).
2. WHEN `Action::evolve(card_id, slot_index)` factory helper is called THEN it SHALL return a correctly populated `Action` with `type == ActionType::Evolve`.
3. WHEN `apply_action` processes `ActionType::Evolve` THEN it SHALL call a dedicated `apply_evolve` helper function.

---

### Requirement 4 — `apply_evolve` Effect Resolution

**User Story:** As a simulator developer, I want `apply_evolve` to correctly mutate game state when a player evolves a Pokemon, preserving all in-play properties of the pre-evolution.

#### Acceptance Criteria

1. WHEN `apply_evolve` is called THEN the evolution card SHALL be removed from the acting player's hand.
2. WHEN `apply_evolve` is called THEN the target slot SHALL be replaced with a new `InPlayPokemon` whose `card` is the evolution card.
3. WHEN `apply_evolve` is called THEN the new `InPlayPokemon` SHALL inherit `attached_energy`, `attached_tool`, and `damage_counters` from the pre-evolution slot.
4. WHEN `apply_evolve` is called THEN `cards_behind` of the new `InPlayPokemon` SHALL equal the old `cards_behind` with the old top card appended.
5. WHEN `apply_evolve` is called THEN `played_this_turn` on the new `InPlayPokemon` SHALL be set to `false` (evolution itself does not count as "placed this turn").
6. WHEN `apply_evolve` is called THEN any volatile status conditions (Paralyzed, Confused, Asleep, Burned) on the pre-evolution SHALL be cleared on the evolved Pokemon (evolution cures volatile status).
7. WHEN `apply_evolve` is called with an invalid target (wrong `evolves_from`, slot empty, or slot occupied by a Pokemon placed this turn) THEN the function SHALL `assert`-fail with a descriptive message.

---

### Requirement 5 — Move Generation: Legal Evolution Actions

**User Story:** As a simulator developer, I want `generate_legal_moves` to include `Evolve` actions for every valid evolution target in the current player's hand and in-play slots, so that AI and human players can discover and choose evolution moves.

#### Acceptance Criteria

1. WHEN generating legal moves THEN for each Stage 1 or Stage 2 card in the current player's hand, the system SHALL check every occupied in-play slot of that player.
2. WHEN a hand card's `evolves_from` matches the name of an in-play Pokemon AND that in-play Pokemon's `stage` is exactly one less than the hand card's `stage` THEN an `Evolve` action SHALL be generated for that (hand card, slot) pair.
3. WHEN it is the current player's first turn (turn 1 for that player, i.e. `turn_number == 1` for player 0 or the equivalent first turn for player 1) THEN NO `Evolve` actions SHALL be generated.
4. WHEN an in-play Pokemon has `played_this_turn == true` THEN NO `Evolve` action SHALL be generated targeting that slot.
5. WHEN an in-play Pokemon has `played_this_turn == false` AND the turn restriction is satisfied THEN an `Evolve` action SHALL be generated if the hand card is a valid evolution.
6. WHEN a player has no Stage 1 or Stage 2 cards in hand THEN no `Evolve` actions SHALL be generated.

---

### Requirement 6 — Knockout Discard: Full Evolution Chain

**User Story:** As a player, I want all cards in an evolved Pokemon's evolution chain to be sent to the discard pile when it is knocked out, matching official PTCGP rules.

#### Acceptance Criteria

1. WHEN `resolve_knockouts` processes a knocked-out evolved Pokemon THEN it SHALL iterate `cards_behind` and push each card to the owner's `discard_pile` before pushing the top card.
2. WHEN `resolve_knockouts` processes a knocked-out Basic Pokemon (no evolution) THEN it SHALL push only the single `card` (and any attached tool) to the discard pile (existing behaviour preserved).
3. WHEN a knocked-out Pokemon has an attached tool THEN the tool card SHALL also be moved to the discard pile (existing behaviour preserved, now also applies to evolved Pokemon).
4. WHEN a knocked-out Pokemon has attached energy THEN the energy cards SHALL NOT be added to the discard pile (energy is simply removed from play, consistent with existing behaviour).

---

### Requirement 7 — Unit Tests

**User Story:** As a developer, I want comprehensive unit tests for the evolution feature so that regressions are caught early.

#### Acceptance Criteria

1. WHEN a Stage 1 card whose `evolves_from` matches an in-play Basic is in hand AND `played_this_turn == false` THEN `generate_legal_moves` SHALL include an `Evolve` action for that slot.
2. WHEN a Stage 1 card is in hand but the matching Basic has `played_this_turn == true` THEN `generate_legal_moves` SHALL NOT include an `Evolve` action for that slot.
3. WHEN it is the player's first turn THEN `generate_legal_moves` SHALL NOT include any `Evolve` actions.
4. WHEN `apply_evolve` is called THEN the evolution card SHALL no longer be in the player's hand.
5. WHEN `apply_evolve` is called THEN the target slot SHALL hold the evolution card with the pre-evolution's energy, tool, and damage counters preserved.
6. WHEN `apply_evolve` is called THEN `cards_behind` SHALL contain the pre-evolution card (and any cards that were already behind it).
7. WHEN an evolved Pokemon (Stage 1 over a Basic) is knocked out THEN both the Stage 1 card and the Basic card SHALL appear in the owner's discard pile.
8. WHEN a Stage 2 Pokemon (evolved from Stage 1 which evolved from Basic) is knocked out THEN all three cards SHALL appear in the owner's discard pile.
