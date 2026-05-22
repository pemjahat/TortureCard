# Requirements Document: Additional Supporter Effects

## Introduction

This feature implements runtime effects for six additional Supporter cards in `supporter_effects.cpp`, following the established pattern from Sabrina and Cyrus. All six cards use the generic "always legal" move-generation path (no special preconditions beyond the once-per-turn supporter rule), so the primary work is effect resolution and the new `GameState` fields needed to carry turn-scoped modifiers.

### Card Identities & Effect Texts (from `supporter_mechanics.json`)

| Card | Card IDs | Effect |
|---|---|---|
| **Professor's Research** | A4b 373, P-A 007 | Draw 2 cards. |
| **Giovanni** | A1 223, A1 270 | During this turn, attacks used by your Pokémon do +10 damage to your opponent's Active Pokémon. |
| **Leaf** | A1a 068, A1a 082 | During this turn, the Retreat Cost of your Active Pokémon is 2 less. |
| **Mars** | A2 155, A2 195 | Your opponent shuffles their hand into their deck and draws a card for each of their remaining points needed to win. |
| **Copycat** | B1 225, B1 270 | Shuffle your hand into your deck. Draw a card for each card in your opponent's hand. |
| **Lisia** | B1 226, B1 271 | Put 2 random Basic Pokémon with 50 HP or less from your deck into your hand. |

---

## Requirements

### Requirement 1 — Professor's Research: Draw 2 Cards

**User Story:** As a player, I want playing Professor's Research to draw 2 cards from the top of my deck into my hand, so I can refuel my hand mid-game.

#### Acceptance Criteria

1. WHEN Professor's Research (`A4b 373` or `P-A 007`) is played THEN the system SHALL move the top 2 cards of the current player's deck into their hand.
2. WHEN the deck has fewer than 2 cards THEN the system SHALL draw only as many cards as remain (0 or 1), without error.
3. WHEN Professor's Research is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile (standard supporter behaviour, already handled by `apply_action`).

---

### Requirement 2 — Giovanni: +10 Damage This Turn

**User Story:** As a player, I want playing Giovanni to boost all my attacks by +10 damage for the rest of this turn, so I can push for a knockout.

#### Acceptance Criteria

1. WHEN Giovanni (`A1 223` or `A1 270`) is played THEN `GameState` SHALL have its `attack_boost` field incremented by `10` (scoped to the current turn).
2. WHEN `attack_boost > 0` THEN `apply_attack_damage` SHALL add `attack_boost` to the final damage dealt to the opponent's Active Pokémon, applied after weakness but before any damage cap.
3. WHEN `reset_turn_flags()` is called at the end of the turn THEN `attack_boost` SHALL be reset to `0`.
4. WHEN Giovanni is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile.

---

### Requirement 3 — Leaf: Reduce Retreat Cost This Turn

**User Story:** As a player, I want playing Leaf to reduce my Active Pokémon's Retreat Cost by 2 for this turn, so I can retreat a heavy Pokémon that would otherwise be stuck.

#### Acceptance Criteria

1. WHEN Leaf (`A1a 068` or `A1a 082`) is played THEN `GameState` SHALL have its `retreat_reduction` field incremented by `2` (scoped to the current turn).
2. WHEN `generate_legal_moves` evaluates Retreat legality THEN it SHALL subtract `retreat_reduction` from the Active Pokémon's retreat cost count (minimum 0) before checking whether the player has sufficient attached energy.
3. WHEN `apply_action` processes a `Retreat` action THEN it SHALL discard `max(0, cost - retreat_reduction)` energy cards instead of the full retreat cost.
4. WHEN `reset_turn_flags()` is called THEN `retreat_reduction` SHALL be reset to `0`.
5. WHEN Leaf is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile.

---

### Requirement 4 — Mars: Opponent Reshuffles Hand, Draws Based on Remaining Points

**User Story:** As a player, I want playing Mars to disrupt the opponent's hand by forcing them to shuffle it back and redraw based on how many points they still need to win, so I can deny them key cards late in the game.

#### Acceptance Criteria

1. WHEN Mars (`A2 155` or `A2 195`) is played THEN the system SHALL shuffle all cards in the **opponent's** hand back into their deck.
2. AFTER the reshuffle THEN the system SHALL draw cards from the opponent's deck equal to `(3 - opponent.points)` — i.e. the number of prize points the opponent still needs to win.
3. WHEN the opponent's deck has fewer cards than the draw count THEN the system SHALL draw only as many cards as remain.
4. WHEN Mars is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile.

---

### Requirement 5 — Copycat: Shuffle Hand, Draw Equal to Opponent's Hand Size

**User Story:** As a player, I want playing Copycat to let me shuffle my hand and draw as many cards as the opponent currently holds, so I can match their hand size when they have a large hand.

#### Acceptance Criteria

1. WHEN Copycat (`B1 225` or `B1 270`) is played THEN the system SHALL record the **opponent's current hand size** before any state changes.
2. THEN the system SHALL shuffle all cards in the **current player's** hand back into their deck (the Copycat card itself has already been removed from hand by `apply_action` before `apply_supporter_effect` is called).
3. THEN the system SHALL draw cards from the current player's deck equal to the recorded opponent hand size.
4. WHEN the player's deck has fewer cards than the draw count THEN the system SHALL draw only as many cards as remain.
5. WHEN Copycat is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile.

---

### Requirement 6 — Lisia: Search 2 Random Basic Pokémon with ≤ 50 HP

**User Story:** As a player, I want playing Lisia to fetch 2 random Basic Pokémon with 50 HP or less from my deck into my hand, so I can quickly fill my bench with small support Pokémon.

#### Acceptance Criteria

1. WHEN Lisia (`B1 226` or `B1 271`) is played THEN the system SHALL collect all Basic Pokémon (`stage == 0`) with `hp <= 50` from the current player's deck.
2. WHEN at least 2 qualifying cards exist THEN the system SHALL randomly select 2 of them (without replacement) and move them to the player's hand.
3. WHEN only 1 qualifying card exists THEN the system SHALL move that 1 card to the player's hand.
4. WHEN no qualifying cards exist THEN the system SHALL perform no draw (no crash).
5. AFTER the search THEN the deck SHALL be shuffled.
6. WHEN Lisia is played THEN `supporter_played_this_turn` SHALL be set to `true` and the card SHALL be moved to the discard pile.

---

### Requirement 7 — Generic Turn-Scoped Modifier Fields in GameState

**User Story:** As the game engine, I need `GameState` to carry generic turn-scoped modifier fields so that attack damage and retreat logic can be boosted or reduced by any card source — not just specific supporters.

#### Acceptance Criteria

1. WHEN `GameState` is defined THEN it SHALL include an `int attack_boost{0}` field in the per-turn flags section, representing bonus damage added to all attacks this turn from any source.
2. WHEN `GameState` is defined THEN it SHALL include an `int retreat_reduction{0}` field in the per-turn flags section, representing the reduction to the Active Pokémon's retreat cost from any source.
3. WHEN `reset_turn_flags()` is called THEN both `attack_boost` and `retreat_reduction` SHALL be reset to `0`.
4. WHEN multiple cards or effects set these fields in the same turn THEN the values SHALL accumulate (additive), not overwrite each other.

---

### Requirement 8 — Unit Tests

**User Story:** As a developer, I want unit tests for all six new supporter effects so regressions are caught automatically.

#### Acceptance Criteria

1. WHEN Professor's Research is played with ≥ 2 deck cards THEN the test SHALL verify exactly 2 cards are added to hand and removed from deck.
2. WHEN Professor's Research is played with 1 deck card THEN the test SHALL verify 1 card is drawn without error.
3. WHEN Giovanni is played THEN the test SHALL verify `attack_boost == 10` and that a subsequent attack deals +10 extra damage compared to without Giovanni.
4. WHEN Leaf is played THEN the test SHALL verify `retreat_reduction == 2` and that a Pokémon with retreat cost 2 can retreat for free (0 energy discarded).
5. WHEN Mars is played THEN the test SHALL verify the opponent's hand is cleared and refilled with `(3 - opponent.points)` cards.
6. WHEN Copycat is played THEN the test SHALL verify the player's hand is replaced with a number of cards equal to the opponent's hand size at the time of playing.
7. WHEN Lisia is played with qualifying Pokémon in deck THEN the test SHALL verify up to 2 Basic Pokémon with HP ≤ 50 are moved to hand and the deck is shuffled.
8. WHEN Lisia is played with no qualifying Pokémon THEN the test SHALL verify no crash and hand size is unchanged.
