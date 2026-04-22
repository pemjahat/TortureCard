# Requirements Document: Game Loop Simulation Tests

## Introduction

This feature adds a dedicated unit test file (`tests/test_game_loop.cpp`) that verifies the correctness of the `GameLoop`, `AttachAttackPlayer`, and `Simulator` components introduced in the game loop simulation feature. Tests follow the same `REQUIRE` / `RUN_TEST` / `g_failures` pattern used throughout the project (no external framework). A new CMake target `ptcgp_test_game_loop` is registered alongside the existing test targets.

The tests are organized around five areas:
1. **AttachAttackPlayer decision logic** — verifying the strategy selects the right action in each scenario.
2. **Setup phase** — verifying both players correctly place their active (and bench) Pokémon before turn 1.
3. **Turn phase sequencing** — verifying Draw → Action → Cleanup → (next player) transitions happen correctly.
4. **Energy generation** — verifying energy is generated at the right turns and for the right player.
5. **Full game loop outcomes** — verifying the game ends with the correct winner, turn count, and point totals under controlled conditions.

---

## Requirements

### Requirement 1: AttachAttackPlayer Decision Logic

**User Story:** As a developer, I want unit tests for `AttachAttackPlayer::decide` so that I can verify the strategy always picks the correct action given the current game state.

#### Acceptance Criteria

1. WHEN the active Pokémon has no energy AND `AttachEnergy` to slot 0 is in the legal moves THEN the player SHALL return the `AttachEnergy` action.
2. WHEN the active Pokémon already satisfies the energy cost of at least one attack AND an `Attack` action is in the legal moves THEN the player SHALL return the `Attack` action (not `AttachEnergy`).
3. WHEN no `AttachEnergy` to slot 0 is available AND no `Attack` is available THEN the player SHALL return the first action in the legal moves list (fallback).
4. WHEN only a `Pass` action is available THEN the player SHALL return `Pass`.
5. WHEN multiple attack indices are available THEN the player SHALL return the first `Attack` action (index 0).

---

### Requirement 2: Setup Phase Behavior

**User Story:** As a developer, I want tests that verify the setup phase correctly places Pokémon for both players before the main game loop begins.

#### Acceptance Criteria

1. WHEN `GameLoop::run` is called with a fresh `GameState` THEN both players SHALL have their active slot (slot 0) occupied after the setup phase completes.
2. WHEN a player's hand contains multiple Basic Pokémon during setup THEN the `AttachAttackPlayer` SHALL place one as the active and the rest as bench Pokémon (up to 3 bench slots).
3. WHEN setup completes THEN `gs.turn_phase` SHALL be `TurnPhase::Draw` and `gs.current_player` SHALL be `0`.
4. WHEN setup completes THEN `gs.turn_number` SHALL be `1`.

---

### Requirement 3: Turn Phase Sequencing

**User Story:** As a developer, I want tests that verify the game loop advances through Draw → Action → Cleanup → (next player's Draw) in the correct order.

#### Acceptance Criteria

1. WHEN a player passes during the Action phase THEN the loop SHALL advance to Cleanup without applying an attack.
2. WHEN a player attacks during the Action phase THEN `gs.attacked_this_turn` SHALL be `true` after the action is applied.
3. WHEN the Cleanup phase completes THEN `gs.current_player` SHALL switch to the other player.
4. WHEN the Cleanup phase completes THEN `gs.turn_number` SHALL increment by 1.
5. WHEN the Draw phase runs AND the current player's deck is non-empty THEN the player's hand size SHALL increase by 1.
6. WHEN the Draw phase runs AND the current player's deck is empty THEN the hand size SHALL remain unchanged and the game SHALL continue without error.

---

### Requirement 4: Energy Generation

**User Story:** As a developer, I want tests that verify energy is generated at the correct turn and for the correct player.

#### Acceptance Criteria

1. WHEN `turn_number == 1` (player 0's first turn) THEN `gs.current_energy` SHALL remain `nullopt` after the Action phase begins (no energy on turn 1).
2. WHEN `turn_number >= 2` AND `gs.current_energy` is `nullopt` THEN the loop SHALL set `gs.current_energy` to a valid energy type from the current player's deck before calling `decide`.
3. WHEN the current player's deck has exactly one energy type THEN the generated energy SHALL always match that type.
4. WHEN `AttachEnergy` is applied THEN `gs.current_energy` SHALL become `nullopt` and `gs.energy_attached_this_turn` SHALL be `true`.
5. WHEN `gs.reset_turn_flags()` is called (at the start of a new turn) THEN `gs.current_energy` SHALL be `nullopt`.

---

### Requirement 5: Full Game Loop Outcomes

**User Story:** As a developer, I want end-to-end tests that run a complete game and verify the winner, turn count, and point totals are correct under controlled, deterministic conditions.

#### Acceptance Criteria

1. WHEN both players have only one Pokémon each (no bench) AND one player's Pokémon is knocked out THEN `SimulationResult::winner` SHALL be the attacking player and `gs.game_over` SHALL be `true`.
2. WHEN a player accumulates 3 points THEN the game SHALL end with that player as the winner, regardless of remaining bench Pokémon.
3. WHEN the game ends THEN `SimulationResult::turns` SHALL equal `gs.turn_number` at the time of game over.
4. WHEN the turn limit (200) is reached without a winner THEN `SimulationResult::winner` SHALL be `-1` (draw) and `gs.game_over` SHALL be `true`.
5. WHEN a bench Pokémon is promoted after the active is knocked out THEN the promoted Pokémon SHALL occupy slot 0 AND the bench slot SHALL be empty AND `played_this_turn` SHALL be `false` on the promoted Pokémon.
6. WHEN status condition Poison is applied to the active Pokémon THEN the Cleanup phase SHALL add 10 damage counters to that Pokémon each turn it remains poisoned.
7. WHEN a Poisoned Pokémon's damage counters reach its HP during Cleanup THEN it SHALL be knocked out and points SHALL be awarded correctly.
8. WHEN `Simulator::run` is called with two valid decks THEN it SHALL return a `SimulationResult` with `winner` equal to 0, 1, or -1 and `turns` greater than 0.
