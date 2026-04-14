# Requirements Document: Game State System

## Introduction

This feature implements the core game state system for TortureCard — a C++ PTCG Pocket simulator. The goal is to define all data structures and logic needed to represent a complete, in-progress game between two players, including their Pokémon on the mat, hands, decks, discard piles, and point counts. It also introduces a `--simulate_turn` CLI command for testing, and adds the `arceusdialga` example deck in TortureCard's JSON format.

The design is informed by the deckgym-core reference implementation (Rust), which uses a flat `State` struct with two-player arrays for hands, decks, discard piles, and a `[4]` slot array per player for in-play Pokémon (index 0 = active, 1–3 = bench).

---

## Requirements

### Requirement 1: Player State Tracking

**User Story:** As a developer, I want each player to have a fully tracked game state (deck, hand, discard pile, point count, and Pokémon slots), so that the game engine can correctly represent both sides of a match.

#### Acceptance Criteria

1. WHEN the game is initialized THEN the system SHALL assign exactly 2 players, each with a reference to their loaded `Deck`.
2. WHEN a player's state is created THEN the system SHALL initialize: a `hand` (vector of `Card`), a `discard_pile` (vector of `Card`), a `points` counter (int, 0–3), and a `pokemon_slots` array of 4 optional `InPlayPokemon` slots.
3. WHEN accessing player Pokémon slots THEN slot index 0 SHALL be the Active Pokémon and slots 1–3 SHALL be Bench slots.
4. IF a slot is empty THEN the system SHALL represent it as a null/empty optional (not a dangling pointer).
5. WHEN a player's deck is assigned THEN the system SHALL store a copy of the `Deck` struct (not a pointer) to allow independent shuffling and drawing.

---

### Requirement 2: In-Play Pokémon (PlayedPokemon) Data Structure

**User Story:** As a developer, I want a dedicated `InPlayPokemon` struct that wraps a `Card` with runtime battle state, so that energy, damage, and status conditions can be tracked independently per Pokémon on the mat.

#### Acceptance Criteria

1. WHEN a Pokémon is placed on the mat THEN the system SHALL create an `InPlayPokemon` wrapping the base `Card` with the following fields:
   - `damage_counters` (int, default 0)
   - `attached_energy` (vector of `EnergyType`)
   - `status` (enum: `None`, `Poisoned`, `Asleep`, `Paralyzed`, `Burned`, `Confused`)
   - `played_this_turn` (bool, default true)
2. WHEN `damage_counters` equals or exceeds the Pokémon's `hp` THEN the system SHALL consider that Pokémon knocked out.
3. WHEN a Pokémon is moved to the bench or discarded THEN the system SHALL clear volatile status conditions (Paralyzed, Confused, Asleep, Burned) but retain damage counters and attached energy.
4. WHEN energy is attached THEN the system SHALL append the `EnergyType` to `attached_energy`; when energy is discarded THEN the system SHALL remove it from the vector.
5. IF a Pokémon has a status condition THEN the system SHALL only allow one status condition at a time (new status replaces old).

---

### Requirement 3: Game State Data Structure

**User Story:** As a developer, I want a `GameState` struct that captures the complete snapshot of a game at any point in time, so that the game engine can advance turns and the CLI can display state.

#### Acceptance Criteria

1. WHEN a `GameState` is created THEN it SHALL contain:
   - `players[2]` — array of two `PlayerState` structs
   - `turn_number` (int, starts at 1)
   - `turn_phase` (enum: `Setup`, `Draw`, `Action`, `Attack`, `Cleanup`)
   - `current_player` (int: 0 or 1)
   - `game_over` (bool, default false)
   - `winner` (int: -1 = none, 0 = player 0, 1 = player 1)
2. WHEN the game starts THEN `turn_number` SHALL be 1, `current_player` SHALL be 0, and `turn_phase` SHALL be `Setup`.
3. WHEN a turn advances THEN `turn_number` SHALL increment and `current_player` SHALL toggle between 0 and 1.
4. WHEN `turn_phase` transitions THEN it SHALL follow the order: `Draw` → `Action` → `Attack` → `Cleanup` → (next turn) `Draw`.
5. WHEN a player reaches 3 points THEN `game_over` SHALL be set to true and `winner` SHALL be set to that player's index.

---

### Requirement 4: `--simulate_turn` CLI Command

**User Story:** As a developer, I want a `ptcgp_cli util --simulate_turn` command that loads two decks, initializes a `GameState`, and prints the resulting state after one simulated turn, so that I can verify the game state system works end-to-end.

#### Acceptance Criteria

1. WHEN `ptcgp_cli util --simulate_turn <deck1.json> <deck2.json>` is invoked THEN the system SHALL:
   - Load both decks from JSON using the existing `Deck::load_from_json` and `Database::load` APIs.
   - Initialize a `GameState` with both decks assigned to `players[0]` and `players[1]`.
   - Print a summary of the initial game state (turn number, phase, each player's hand size, active Pokémon slot status).
2. WHEN the command runs THEN the system SHALL NOT crash if either deck is valid per `Deck::validate`.
3. IF a deck file path is missing or invalid THEN the system SHALL print an error message and return exit code 1.
4. WHEN the state is printed THEN the output SHALL include for each player: hand size, discard pile size, points, and whether the active slot is occupied.

---

### Requirement 5: `arceusdialga.json` Example Deck

**User Story:** As a developer, I want the `arceusdialga` deck from deckgym-core imported into TortureCard's `decks/` folder in the correct JSON format, so that it can be used as a test deck for the `--simulate_turn` command.

#### Acceptance Criteria

1. WHEN the file `decks/arceusdialga.json` is created THEN it SHALL follow the same JSON schema as `decks/altaria.json`: `{ "energy": [...], "cards": [{ "id": "...", "count": N, "name": "..." }, ...] }`.
2. WHEN the deck is loaded THEN `Deck::validate` SHALL pass (20 cards total, max 2 copies per card).
3. WHEN the deck is loaded THEN the `energy` field SHALL be `["Metal"]` matching the source `arceusdialga.txt`.
4. WHEN the deck entries are converted THEN each line from `arceusdialga.txt` (format: `<count> <expansion> <number>`) SHALL map to a JSON entry with the correct `id` (e.g. `"A1 223"`), `count`, and a `name` field populated from the card database where possible (or `"(unknown)"` as fallback).

---

### Requirement 6: Existing `game_state.h` Refactoring

**User Story:** As a developer, I want the existing stub `game_state.h` to be replaced with the full `GameState` and `InPlayPokemon` definitions, so that the new structures are available across the codebase.

#### Acceptance Criteria

1. WHEN `game_state.h` is updated THEN it SHALL define `InPlayPokemon`, `TurnPhase` enum, and the expanded `PlayerState` and `GameState` structs.
2. WHEN `game_state.h` is updated THEN it SHALL remain compatible with the existing `#include "ptcgp_sim.h"` umbrella header.
3. WHEN the existing `PlayerState::bench` (vector) and `PlayerState::active` (raw pointer) fields are replaced THEN the new `pokemon_slots[4]` array of `std::optional<InPlayPokemon>` SHALL be used instead.
4. WHEN the `GameState` is updated THEN the existing `turn` field SHALL be renamed to `turn_number` and a `turn_phase` field SHALL be added.
