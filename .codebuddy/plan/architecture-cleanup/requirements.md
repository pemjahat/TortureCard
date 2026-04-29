# Architecture Cleanup Requirements

## Introduction

The TortureCard (`ptcgp_sim`) codebase has grown organically through a series of feature plans (game-state, game-loop, attack/ability mechanics, evolution, ex/mega points, etc.). The implementation is functionally solid, but several categories of technical debt have accumulated:

1. **Boilerplate duplication** — every test file independently re-declares identical `REQUIRE` / `RUN_TEST` macros, `make_pokemon` / `make_deck` / `make_game` helpers, and the `g_failures` counter.
2. **Unimplemented extension points** — `UnknownMechanic` / `UnknownAbilityMechanic` silently swallow unrecognised effect text; several `ActionType` variants (`PlaySupporter`, `PlayStadium`, `PlayItem` beyond Poke Ball) have no real effect resolution; several `PassiveHook` values (`EndOfTurn`, `RetreatCost`, `GameState`, `OnEvolve`, `OnBenchPlay`) are declared but never consulted.
3. **Test consolidation** — many tests across different files exercise the same underlying helpers (`make_game`, `make_deck`, coin-seed finders) and could share a common test utility header, reducing copy-paste drift.
4. **Minor structural issues** — the `RUN_TEST` macro in `test_attack_effects.cpp` does not print `[PASS]` on success (inconsistent with all other test files); `HealOneYourPokemon::apply_activate` has a documented simplification ("heals the ability user's own slot") that needs a proper target-selection hook; the `main.cpp` CLI has repeated deck-validation boilerplate across three subcommands.

This cleanup plan addresses all four categories without changing any observable game logic.

---

## Requirements

### Requirement 1 — Shared Test Infrastructure Header

**User story:** As a developer, I want a single shared test utility header so that I do not have to copy-paste `REQUIRE`, `RUN_TEST`, `g_failures`, and common card/deck/game factory helpers into every test file.

#### Acceptance Criteria

1. WHEN a new `tests/test_helpers.h` header is created THEN it SHALL define the `REQUIRE(expr)` macro, the `RUN_TEST(func)` macro (with consistent `[PASS]` / `[FAIL]` output), and the `g_failures` counter.
2. WHEN `test_helpers.h` is created THEN it SHALL provide `make_pokemon(...)`, `make_trainer(...)`, `make_attack(...)`, `make_deck(cards, energy)`, and `make_game(p0_active, p1_active, ...)` factory functions that cover the superset of signatures currently duplicated across all test files.
3. WHEN each existing test file is updated THEN it SHALL `#include "test_helpers.h"` and remove its own local copies of the above macros and helpers.
4. WHEN `test_attack_effects.cpp` is updated THEN its `RUN_TEST` macro SHALL print `[PASS]` on success, matching the behaviour of all other test files.
5. IF a test file requires a specialised helper not present in `test_helpers.h` THEN that helper SHALL remain local to that file.

---

### Requirement 2 — Unimplemented Attack Mechanic Extension Points

**User story:** As a developer, I want unimplemented attack effect text to be clearly surfaced at load time so that I can track which cards are falling back to `UnknownMechanic` and prioritise implementing them.

#### Acceptance Criteria

1. WHEN `Database::resolve_mechanics()` encounters an attack effect text that is not in `attack_mechanic_dictionary` THEN the system SHALL log a warning (to `std::cerr`) containing the card name and the unrecognised effect text.
2. WHEN `UnknownMechanic::compute_damage` is called at runtime THEN the system SHALL NOT crash and SHALL fall back to `fixed_damage` (existing behaviour preserved).
3. WHEN a new `AttackMechanic` concrete type is added THEN it SHALL follow the existing pattern: `compute_damage`, `equals`, `clone`, `type_name`, `params_json`, `from_params_json`.
4. IF the `attack_mechanic_dictionary` already contains an entry for an effect text THEN no warning SHALL be emitted for that text.

---

### Requirement 3 — Unimplemented Ability Mechanic Extension Points

**User story:** As a developer, I want unimplemented ability effect text to be clearly surfaced and the `PassiveHook` enum values that are declared but never consulted to be documented as future extension points.

#### Acceptance Criteria

1. WHEN `Database::resolve_mechanics()` encounters an ability effect text that is not in `ability_mechanic_dictionary` THEN the system SHALL log a warning (to `std::cerr`) containing the card name and the unrecognised ability text.
2. WHEN `UnknownAbilityMechanic` is used as a fallback THEN it SHALL NOT crash and SHALL remain a no-op (existing behaviour preserved).
3. WHEN `HealOneYourPokemon::apply_activate` is called THEN it SHALL heal the Pokemon in `slot_idx` (current deterministic behaviour); a `// TODO: present target-selection choice` comment SHALL be added to mark the future interactive hook.
4. WHEN `PassiveHook` values `EndOfTurn`, `RetreatCost`, `GameState`, `OnEvolve`, `OnBenchPlay` are declared THEN each SHALL have a `// TODO: hook not yet consulted` comment in `ability_mechanic.h` to document the gap.
5. IF a new `AbilityMechanic` concrete type is added THEN it SHALL follow the existing pattern: `timing`, `passive_hook`, `apply_activate`, `equals`, `clone`, `type_name`, `params_json`, `from_params_json`.

---

### Requirement 4 — Unimplemented Action Type Resolution

**User story:** As a developer, I want every `ActionType` variant to have a clearly documented implementation status in `effects.h` / `apply_action`, so that I know which actions are stubs and which are fully resolved.

#### Acceptance Criteria

1. WHEN `apply_action` handles `ActionType::PlaySupporter` THEN it SHALL set `supporter_played_this_turn = true` and remove the card from hand (current behaviour); a `// TODO: resolve supporter effect` comment SHALL be added to mark the unimplemented effect body.
2. WHEN `apply_action` handles `ActionType::PlayStadium` THEN it SHALL set `current_stadium` and remove the card from hand (current behaviour); a `// TODO: resolve stadium effect` comment SHALL be added.
3. WHEN `apply_action` handles `ActionType::PlayItem` THEN the Poke Ball branch SHALL remain implemented; all other item names SHALL fall through to a discard-only path with a `// TODO: resolve item effect for <name>` log line to `std::cerr`.
4. WHEN `apply_action` handles `ActionType::UseAbility` THEN it SHALL delegate to `apply_ability_action` (current behaviour); the function header comment SHALL list which `AbilityTiming::Passive` hooks are not yet wired.
5. IF a new `ActionType` variant is added in the future THEN the `apply_action` switch SHALL produce a compile-time warning (via `[[fallthrough]]` or `default: assert(false)`) if it is not handled.

---

### Requirement 5 — CLI Boilerplate Consolidation

**User story:** As a developer, I want the repeated deck-loading and validation boilerplate in `main.cpp` to be extracted into a helper function so that each CLI subcommand is concise and consistent.

#### Acceptance Criteria

1. WHEN `main.cpp` is refactored THEN a `load_and_validate_deck(const std::string& path, const Database& db, const std::string& label)` helper function SHALL be extracted that loads a deck, validates it, prints errors to `std::cerr`, and returns the `Deck` (or exits on failure).
2. WHEN the `--simulate_turn`, `--dump_moves`, and `sim` subcommands call deck loading THEN each SHALL call `load_and_validate_deck` instead of repeating the load + validate + error-print pattern inline.
3. WHEN the `--dump_moves` subcommand prints legal moves THEN the `trainer_type_str` lambda and `print_slot` lambda SHALL remain local to that subcommand (they are not shared).
4. IF the database fails to load THEN the CLI SHALL print a descriptive error and return a non-zero exit code (existing behaviour preserved).

---

### Requirement 6 — Test Consolidation: Duplicate `make_game` / Coin-Seed Helpers

**User story:** As a developer, I want coin-flip seed-finding utilities and the `make_game` factory to be shared across test files so that changes to game-state initialisation only need to be made in one place.

#### Acceptance Criteria

1. WHEN `test_helpers.h` is created (Requirement 1) THEN it SHALL include `find_seed_for_heads(int num_coins, int target_heads)` and `find_seed_for_flip_until_tails(int target_heads)` utilities currently duplicated in `test_attack_effects.cpp`.
2. WHEN `test_attack_effects.cpp` is updated THEN it SHALL remove its local `find_seed_for_heads` and `find_seed_for_flip_until_tails` and use the shared versions.
3. WHEN `make_game` in `test_helpers.h` is defined THEN it SHALL accept optional `p0_damage_counters` and `p1_damage_counters` parameters (as in `test_ability_effects.cpp`) so that all test files can use a single factory.
4. WHEN `test_game_loop.cpp` uses `make_mid_game` THEN that function SHALL be renamed to `make_game` and moved to `test_helpers.h`, with the `energy` parameter defaulting to `EnergyType::Fire`.

---

### Requirement 7 — Test Consolidation: Overlapping KO / Points Tests

**User story:** As a developer, I want overlapping knockout-and-points tests that appear in both `test_effect_resolution.cpp` and `test_ex_mega_points.cpp` to be rationalised so that each scenario is tested in exactly one file.

#### Acceptance Criteria

1. WHEN `test_effect_resolution.cpp` and `test_ex_mega_points.cpp` are reviewed THEN tests that cover the same `resolve_knockouts` path (e.g. "KO awards 1 point", "3 points triggers game_over") SHALL be identified.
2. WHEN duplicated KO-point tests are found THEN the canonical version SHALL remain in `test_effect_resolution.cpp` (basic KO mechanics) and the ex/mega-specific variants SHALL remain in `test_ex_mega_points.cpp`.
3. WHEN `test_game_loop.cpp` contains inline re-implementations of `apply_status_damage` logic THEN those inline blocks SHALL be replaced with calls to the actual `GameLoop` or a shared helper, or annotated with `// mirrors GameLoop::apply_status_damage` to make the intent explicit.
4. IF a test is removed as a duplicate THEN the remaining test SHALL cover all the same boundary conditions as the removed test.

---

### Requirement 8 — `InPlayPokemon` Convenience Method Completeness

**User story:** As a developer, I want `InPlayPokemon` to expose a `clear_all_status()` method (clearing both volatile and non-volatile conditions) alongside the existing `clear_volatile_status()`, so that future game-loop cleanup code has a clear API.

#### Acceptance Criteria

1. WHEN `InPlayPokemon::clear_volatile_status()` is called THEN it SHALL clear `Paralyzed`, `Confused`, `Asleep`, and `Burned` (existing behaviour preserved).
2. WHEN a new `InPlayPokemon::clear_all_status()` method is added THEN it SHALL additionally clear `Poisoned`.
3. WHEN `apply_evolve` clears status on evolution THEN it SHALL call `clear_volatile_status()` (existing behaviour; Poisoned persists through evolution per TCG rules).
4. WHEN `GameLoop::run_cleanup_phase` needs to clear all status after a KO THEN it SHALL call `clear_all_status()` on the promoted Pokemon.

