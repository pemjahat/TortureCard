# Requirements Document: Ex & Mega Pokémon Knockout Points

## Introduction

In the Pokémon TCG Pocket rules, not all Pokémon are worth the same number of prize points when knocked out. Regular Pokémon award **1 point**, Ex Pokémon award **2 points**, and Mega Pokémon award **3 points**. The TortureCard simulator currently hard-codes `+= 1` for every knockout in `resolve_knockouts()` inside `effects.cpp`, and the `Card` struct has no way to classify a card as Ex or Mega.

This feature introduces:
1. A **name-based classification** on `Card` to detect Ex and Mega Pokémon (mirroring the deckgym-core approach).
2. A **`knockout_points()` helper** that returns the correct point value (1 / 2 / 3).
3. Updated **`resolve_knockouts()`** to award the correct variable number of points.
4. A **win-condition threshold update** — the game is won at **3 points** (unchanged), but Ex/Mega KOs can reach that threshold faster.
5. **Unit tests** covering all three point tiers and the resulting win conditions.

---

## Requirements

### Requirement 1 — Ex Pokémon Detection

**User Story:** As a simulator developer, I want the `Card` struct to identify Ex Pokémon by name, so that the engine can award the correct prize points without requiring a separate database flag.

#### Acceptance Criteria

1. WHEN a `Card` has `type == CardType::Pokemon` AND its `name` field ends with the suffix `" ex"` (case-insensitive, space-separated last word) THEN `card.is_ex()` SHALL return `true`.
2. WHEN a `Card`'s name does NOT end with `" ex"` THEN `card.is_ex()` SHALL return `false`.
3. WHEN a `Card` has `type == CardType::Trainer` THEN `card.is_ex()` SHALL return `false` regardless of name.
4. IF the name is exactly `"ex"` (no preceding word) THEN `card.is_ex()` SHALL return `false` (must have at least one word before the suffix).

---

### Requirement 2 — Mega Pokémon Detection

**User Story:** As a simulator developer, I want the `Card` struct to identify Mega Pokémon by name, so that the engine can award 3 prize points on knockout.

#### Acceptance Criteria

1. WHEN a `Card` has `type == CardType::Pokemon` AND its `name` field starts with the prefix `"Mega "` (capital M, followed by a space) THEN `card.is_mega()` SHALL return `true`.
2. WHEN a `Card`'s name does NOT start with `"Mega "` THEN `card.is_mega()` SHALL return `false`.
3. WHEN a `Card` has `type == CardType::Trainer` THEN `card.is_mega()` SHALL return `false` regardless of name.

---

### Requirement 3 — Knockout Points Helper

**User Story:** As a simulator developer, I want a single `knockout_points()` method on `Card` that returns the prize point value for knocking out that Pokémon, so that all point-awarding logic has one authoritative source.

#### Acceptance Criteria

1. WHEN `card.is_mega()` is `true` THEN `card.knockout_points()` SHALL return `3`.
2. WHEN `card.is_mega()` is `false` AND `card.is_ex()` is `true` THEN `card.knockout_points()` SHALL return `2`.
3. WHEN both `card.is_mega()` and `card.is_ex()` are `false` THEN `card.knockout_points()` SHALL return `1`.
4. IF a card is both Mega and Ex by name (e.g. `"Mega Charizard ex"`) THEN `card.knockout_points()` SHALL return `3` (Mega takes priority).
5. WHEN `card.type == CardType::Trainer` THEN `card.knockout_points()` SHALL return `0` (Trainer cards cannot be knocked out).

---

### Requirement 4 — Variable Prize Points in resolve_knockouts()

**User Story:** As a game engine developer, I want `resolve_knockouts()` to award the correct number of prize points based on the knocked-out Pokémon's classification, so that Ex and Mega KOs accelerate the game toward the win condition correctly.

#### Acceptance Criteria

1. WHEN a regular (non-Ex, non-Mega) Pokémon is knocked out THEN the opponent SHALL receive exactly `1` prize point.
2. WHEN an Ex Pokémon is knocked out THEN the opponent SHALL receive exactly `2` prize points.
3. WHEN a Mega Pokémon is knocked out THEN the opponent SHALL receive exactly `3` prize points.
4. WHEN multiple Pokémon are knocked out simultaneously (e.g. bench damage) THEN each KO SHALL award its own `knockout_points()` value independently.
5. WHEN the awarded points cause a player's total to reach or exceed `3` THEN `game_over` SHALL be set to `true` and `winner` SHALL be set to that player's index (existing win-condition logic, now triggered by variable point increments).

---

### Requirement 5 — Unit Tests

**User Story:** As a developer, I want comprehensive unit tests for the Ex/Mega classification and point-awarding logic, so that regressions are caught automatically.

#### Acceptance Criteria

1. WHEN `is_ex()` is called on cards with names `"Mewtwo ex"`, `"Charizard ex"`, `"Pikachu ex"` THEN each SHALL return `true`.
2. WHEN `is_ex()` is called on cards with names `"Pikachu"`, `"Charizard"`, `"Mewtwo"` THEN each SHALL return `false`.
3. WHEN `is_mega()` is called on cards with names `"Mega Charizard"`, `"Mega Mewtwo"` THEN each SHALL return `true`.
4. WHEN `is_mega()` is called on cards with names `"Charizard"`, `"Mewtwo ex"` THEN each SHALL return `false`.
5. WHEN `knockout_points()` is called on a regular Pokémon THEN it SHALL return `1`.
6. WHEN `knockout_points()` is called on an Ex Pokémon THEN it SHALL return `2`.
7. WHEN `knockout_points()` is called on a Mega Pokémon THEN it SHALL return `3`.
8. WHEN a regular Pokémon is knocked out in a simulated game THEN the opponent's `points` SHALL increase by `1`.
9. WHEN an Ex Pokémon is knocked out in a simulated game THEN the opponent's `points` SHALL increase by `2`.
10. WHEN a Mega Pokémon is knocked out in a simulated game THEN the opponent's `points` SHALL increase by `3`.
11. WHEN knocking out an Ex Pokémon gives the opponent exactly `3` points THEN `game_over` SHALL be `true` and `winner` SHALL be the opponent's index.
