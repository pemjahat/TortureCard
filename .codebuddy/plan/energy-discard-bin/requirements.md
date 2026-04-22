# Requirements Document: Energy Discard Bin

## Introduction

In the Pokémon TCG Pocket rules, when a Pokémon that has energy attached to it is removed from play — either by being knocked out or by retreating — those attached energies are not simply deleted. They are moved to a separate **energy discard bin** (also called the energy discard pile), which is distinct from the regular card discard pile. This energy discard bin is important for future card mechanics that can retrieve or interact with discarded energies (e.g., cards like Misty, Electrode, or Electrical Cord in deckgym-core).

Currently, in the TortureCard C++ simulator (`d:\TortureCard`), the `PlayerState` struct only has a `discard_pile` for cards. When a Pokémon is knocked out (`resolve_knockouts` in `effects.cpp`) or retreats (`ActionType::Retreat` in `effects.cpp`), the attached energies are simply dropped — they are neither tracked nor moved anywhere. This feature adds a proper `energy_discard` bin to `PlayerState`, and ensures energies are correctly routed to it in both scenarios.

The reference implementation in `d:\deckgym-core` (Rust) uses `discard_energies: [Vec<EnergyType>; 2]` in `State`, populated via `discard_from_play()` on knockout and `discard_from_active()` on retreat/attack-discard. This C++ feature mirrors that design.

---

## Requirements

### Requirement 1 — Energy Discard Bin in PlayerState

**User Story:** As a game engine developer, I want `PlayerState` to track discarded energies separately from discarded cards, so that future card mechanics can query and interact with the energy discard bin.

#### Acceptance Criteria

1. WHEN the game state is initialized THEN `PlayerState` SHALL contain a `std::vector<EnergyType> energy_discard` field initialized to empty.
2. WHEN a `PlayerState` is printed or inspected THEN the `energy_discard` field SHALL be accessible and reflect the current count and types of discarded energies.
3. IF `energy_discard` is empty THEN it SHALL remain an empty vector (not nullopt or any sentinel value).

---

### Requirement 2 — Energy Sent to Discard Bin on Knockout

**User Story:** As a game engine developer, I want all energies attached to a knocked-out Pokémon to be moved to the owner's `energy_discard` bin, so that the game state accurately reflects where those energies went.

#### Acceptance Criteria

1. WHEN `resolve_knockouts` processes a knocked-out Pokémon THEN the system SHALL move all entries in `InPlayPokemon::attached_energy` to the owning player's `PlayerState::energy_discard`.
2. WHEN a knocked-out Pokémon has zero attached energies THEN `energy_discard` SHALL remain unchanged (no entries added).
3. WHEN a knocked-out Pokémon has multiple energies of different types THEN ALL of those energies SHALL be appended to `energy_discard` in the order they appear in `attached_energy`.
4. WHEN a Pokémon with an evolution chain (cards_behind) is knocked out THEN the energies from the top-stage Pokémon (the `InPlayPokemon`) SHALL be discarded — lower-stage cards do not carry energy.
5. WHEN a knocked-out Pokémon also has an attached tool card THEN the tool card SHALL go to `discard_pile` (existing behavior) and the energies SHALL go to `energy_discard` (new behavior), independently.

---

### Requirement 3 — Energy Sent to Discard Bin on Retreat

**User Story:** As a game engine developer, I want the energies paid as retreat cost to be moved to the retreating player's `energy_discard` bin, so that the game state accurately reflects where those energies went.

#### Acceptance Criteria

1. WHEN a `Retreat` action is applied THEN the system SHALL move each energy removed from `InPlayPokemon::attached_energy` (as retreat cost payment) into the retreating player's `PlayerState::energy_discard`.
2. WHEN the retreat cost is zero (free retreat) THEN no energies SHALL be added to `energy_discard`.
3. WHEN the retreat cost is N energies THEN exactly N energies SHALL be appended to `energy_discard` (one per cost unit paid).
4. WHEN a Pokémon retreats with more energy attached than the retreat cost THEN only the energies actually removed (equal to the retreat cost) SHALL be added to `energy_discard`; remaining attached energies SHALL stay on the Pokémon.

---

### Requirement 4 — Energy Discard Bin Visibility in CLI Output

**User Story:** As a developer or tester, I want the energy discard bin to be visible in the CLI `--simulate_turn` and `--dump_moves` output, so that I can verify the state during debugging.

#### Acceptance Criteria

1. WHEN `--simulate_turn` prints the per-player state summary THEN it SHALL include a line showing the count of energies in `energy_discard` (e.g., `Energy discard: 2`).
2. WHEN `--dump_moves` prints the per-player state THEN it SHALL include a line showing the count and types of energies in `energy_discard` (e.g., `Energy discard: [Fire, Water]`).
3. IF `energy_discard` is empty THEN the output SHALL show `Energy discard: (none)` or `Energy discard: 0`.

---

### Requirement 5 — Test Cases

**User Story:** As a developer, I want automated test cases that verify the energy discard bin behavior for both knockout and retreat scenarios, so that regressions are caught early.

#### Acceptance Criteria

1. WHEN a test knocks out a Pokémon with N attached energies THEN the test SHALL assert that the owner's `energy_discard` contains exactly those N energies.
2. WHEN a test knocks out a Pokémon with zero attached energies THEN the test SHALL assert that `energy_discard` remains empty.
3. WHEN a test knocks out a Pokémon with an attached tool THEN the test SHALL assert that the tool is in `discard_pile` and the energies are in `energy_discard`.
4. WHEN a test retreats a Pokémon with a retreat cost of N THEN the test SHALL assert that exactly N energies are added to `energy_discard` and the Pokémon retains its remaining energies.
5. WHEN a test retreats a Pokémon with a free retreat cost (0) THEN the test SHALL assert that `energy_discard` remains empty.
6. WHEN a test knocks out multiple Pokémon in the same `resolve_knockouts` call THEN the test SHALL assert that each owner's `energy_discard` is updated independently and correctly.
7. All new tests SHALL be added to an appropriate existing test file (e.g., `tests/test_effect_resolution.cpp`) or a new dedicated file, following the existing `REQUIRE` / `RUN_TEST` pattern.
