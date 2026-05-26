# Requirements Document: Tool & Item Effect Implementation

## Introduction

This feature implements the in-game effects of three trainer cards — **Rocky Helmet** (Tool), **Giant Cape** (Tool), and **Potion** (Item) — following the same dispatch architecture already established for Supporter cards in `supporter_effects.cpp`.

All new logic is encapsulated in a new source file `src/lib/toolitem_effects.cpp` with a corresponding header `include/ptcgp_sim/toolitem_effects.h`. The existing `apply_action` dispatcher in `effects.cpp` is updated to call into the new layer. The CMakeLists picks up the new `.cpp` automatically via `GLOB_RECURSE`.

### Card Definitions

| Card | Type | Effect |
|---|---|---|
| **Rocky Helmet** | Tool | When the Pokémon this card is attached to is damaged by an opponent's attack, deal 20 damage to the attacking Pokémon. |
| **Giant Cape** | Tool | The Pokémon this card is attached to gets +20 max HP. |
| **Potion** | Item | Heal 20 damage from one of your Pokémon. |

### Known Card IDs (from existing codebase)

- Rocky Helmet: `A1 088` (expansion `"A1"`, number `88`)
- Giant Cape: `A2 147` (expansion `"A2"`, number `147`)
- Potion: to be confirmed — standard item, expansion `"A1"` or `"PA"`

---

## Requirements

### Requirement 1 — New dispatch header and source file

**User Story:** As a developer, I want tool and item effects to live in a dedicated `toolitem_effects.cpp` / `.h` pair, so that the codebase stays modular and the pattern mirrors `supporter_effects`.

#### Acceptance Criteria

1. WHEN the project is built THEN the system SHALL compile `src/lib/toolitem_effects.cpp` as part of `ptcgp_sim_lib` without any changes to `CMakeLists.txt` (picked up by existing `GLOB_RECURSE`).
2. WHEN `toolitem_effects.h` is included THEN the system SHALL expose `apply_item_effect(GameState&, int player, const CardId&, int target_slot, std::mt19937&)` and `apply_tool_passive(GameState&, int attacker_player, int damage_dealt)` as the two public entry points.
3. WHEN `effects.cpp` handles `ActionType::PlayItem` THEN the system SHALL delegate to `apply_item_effect` for all item cards (replacing the current `is_search_basic_item` branch and the `[WARN]` fallback).
4. WHEN `effects.cpp` handles `ActionType::PlayTool` THEN the system SHALL NOT call any effect function (tool effects are passive and triggered at damage-resolution time, not at play time).

---

### Requirement 2 — Rocky Helmet (Tool — counterattack passive)

**User Story:** As a player, I want Rocky Helmet to deal 20 damage back to the attacker whenever my Pokémon is hit, so that aggressive decks are punished.

#### Acceptance Criteria

1. WHEN an opponent's attack deals damage > 0 to a Pokémon that has Rocky Helmet attached THEN the system SHALL deal 20 damage to the attacking Pokémon (applied after the main damage, before KO resolution).
2. WHEN the Rocky Helmet counterattack damage would reduce the attacker's remaining HP to 0 or below THEN the system SHALL cap `damage_counters` at `card.hp` (same clamping rule as all other damage).
3. WHEN the main attack deals 0 damage (e.g. fully reduced by a passive ability) THEN the system SHALL NOT trigger Rocky Helmet.
4. IF the defending Pokémon has Rocky Helmet attached AND is knocked out by the attack THEN the system SHALL still apply the 20 counterattack damage to the attacker before KO resolution.
5. WHEN `apply_tool_passive` is called THEN the system SHALL check all active and bench Pokémon of the defending player for a Rocky Helmet attachment (only the directly-attacked slot triggers counterattack).

---

### Requirement 3 — Giant Cape (Tool — HP bonus passive)

**User Story:** As a player, I want Giant Cape to increase the attached Pokémon's effective HP by 20, so that it survives one extra hit.

#### Acceptance Criteria

1. WHEN a Pokémon has Giant Cape attached THEN the system SHALL treat its effective HP as `card.hp + 20` for the purpose of KO detection (`is_knocked_out()`) and `remaining_hp()`.
2. WHEN Giant Cape is discarded (e.g. on KO) THEN the system SHALL revert to the base `card.hp` for KO detection (the tool is gone, so the bonus disappears).
3. WHEN `is_knocked_out()` is evaluated THEN the system SHALL use a helper `effective_hp(const InPlayPokemon&)` that adds +20 if Giant Cape is attached, so the check is centralised.
4. WHEN `remaining_hp()` is evaluated THEN the system SHALL also use `effective_hp` so that the displayed HP is consistent.
5. IF a Pokémon already has damage counters equal to `card.hp` and Giant Cape is then attached THEN the system SHALL NOT retroactively revive it (Giant Cape is only played onto a living Pokémon per game rules — enforced by move generation).

---

### Requirement 4 — Potion (Item — targeted heal)

**User Story:** As a player, I want to play Potion to heal 20 damage from one of my Pokémon, so that I can keep key Pokémon alive longer.

#### Acceptance Criteria

1. WHEN `ActionType::PlayItem` is resolved for a Potion card THEN the system SHALL reduce `damage_counters` of the target Pokémon by 20, clamped to a minimum of 0.
2. WHEN Potion is played THEN the system SHALL require `action.target_slot` (0–3) to identify which Pokémon is healed; the card is discarded to the player's discard pile (already handled by `apply_action` before calling `apply_item_effect`).
3. WHEN the target Pokémon has fewer than 20 damage counters THEN the system SHALL set `damage_counters` to 0 (no over-heal).
4. WHEN move generation enumerates Potion plays THEN the system SHALL emit one `PlayItem` action per Pokémon slot that has at least 1 damage counter (playing Potion on a full-HP Pokémon is illegal).
5. IF no Pokémon on the player's side has any damage counters THEN the system SHALL NOT emit any `PlayItem` action for Potion.

---

### Requirement 5 — Integration into `apply_attack_damage`

**User Story:** As a developer, I want tool passives (Rocky Helmet) to be triggered automatically inside `apply_attack_damage`, so that no call site is missed.

#### Acceptance Criteria

1. WHEN `apply_attack_damage` finishes applying damage to the defender THEN the system SHALL call `apply_tool_passive(gs, attacker_player, damage)` to trigger any tool-based counterattack.
2. WHEN `apply_tool_passive` is called with `damage == 0` THEN the system SHALL be a no-op (no counterattack triggered).
3. WHEN `apply_tool_passive` applies Rocky Helmet damage THEN the system SHALL NOT call `resolve_knockouts` itself — the existing call in `apply_action` after `apply_attack_damage` returns handles all KOs.

---

### Requirement 6 — Move generation for Potion

**User Story:** As a developer, I want `generate_moves` to correctly enumerate Potion plays only when there is a valid target, so that the AI never generates illegal actions.

#### Acceptance Criteria

1. WHEN generating moves for `TrainerType::Item` cards THEN the system SHALL check if the card is a Potion and, if so, emit one `PlayItem` action per damaged Pokémon slot (slots 0–3) instead of a single unconditional action.
2. WHEN a Potion is in hand but all Pokémon are at full HP THEN the system SHALL emit zero `PlayItem` actions for that Potion.
3. WHEN a non-Potion item is in hand THEN the system SHALL continue to emit a single unconditional `PlayItem` action (existing behaviour preserved).

---

### Requirement 7 — Unit tests in a new test file

**User Story:** As a developer, I want a dedicated test file `tests/test_toolitem_effects.cpp` registered in CMakeLists, so that all tool/item effect behaviour is verified in isolation.

#### Acceptance Criteria

1. WHEN the test suite runs THEN the system SHALL execute tests covering: Rocky Helmet counterattack (normal hit, zero-damage hit, KO scenario), Giant Cape HP bonus (KO threshold raised, KO after tool discarded), and Potion heal (normal heal, over-heal clamp, move generation legality).
2. WHEN `CMakeLists.txt` is updated THEN the system SHALL register the new test target `ptcgp_test_toolitem_effects` using the existing `add_ptcgp_test` macro.
