# Requirements Document: Action & Move Generation System

## Introduction

This feature implements the **Action type system** and **legal move generation** for TortureCard — a C++ PTCG Pocket simulator. The design is ported from the deckgym-core Rust reference implementation, adapted to C++ idioms and the existing TortureCard architecture.

The system introduces:
1. An `Action` enum covering all player-facing moves (Draw, PlayPokemon, AttachEnergy, Attack, Retreat, Pass, PlayTrainer, PlayItem, PlayTool, PlayStadium).
2. A `generate_legal_moves(GameState, player)` function that returns all valid actions for the current game state.
3. Expanded `PokemonCard` data (`stage`) loaded from the database. Evolution mechanics (`evolves_from`) are deferred to a future iteration.
4. Expanded `TrainerCard` data with a `TrainerType` sub-classification: Item, Tool, Supporter, Stadium.
5. A CLI subcommand `--dump_moves` for human-readable verification of legal moves given a game state.
6. Minimum rule set required to make legality checks correct and testable.

The reference implementation in deckgym-core uses `SimpleAction` variants (Place, Evolve, Attach, Attack, Retreat, EndTurn, Play) with a `generate_possible_actions(state)` function. TortureCard will mirror this logic in C++.

---

## Requirements

### Requirement 1: Action Type Enum

**User Story:** As a developer, I want a strongly-typed `Action` enum that covers every legal player move, so that the game engine and AI can reason about moves uniformly.

#### Acceptance Criteria

1. WHEN the `Action` type is defined THEN the system SHALL include the following variants:
   - `Draw` — draw a card from the deck (used during the Draw phase or by card effects)
   - `PlayPokemon { card_id, slot_index }` — place a Basic Pokémon from hand onto an empty slot (0 = active, 1–3 = bench)
   - `AttachEnergy { energy_type, target_slot }` — attach the turn's generated energy to a Pokémon in play
   - `Attack { attack_index }` — use the active Pokémon's attack at the given index
   - `Retreat { to_slot }` — swap the active Pokémon with a bench Pokémon at `to_slot`
   - `Pass` — end the current player's turn without further action
   - `PlaySupporter { card_id }` — play a Supporter card from hand (once per turn)
   - `PlayItem { card_id }` — play an Item card from hand (no per-turn limit)
   - `PlayTool { card_id, target_slot }` — attach a Tool card to a Pokémon in play (one Tool per Pokémon)
   - `PlayStadium { card_id }` — play a Stadium card to replace the current Stadium in play
2. WHEN an `Action` is constructed THEN it SHALL carry only the data needed to apply it (no redundant copies of full card data).
3. WHEN an `Action` is printed THEN the system SHALL produce a human-readable string (e.g., `"Attack(0)"`, `"PlayPokemon(A1 001, slot=1)"`, `"PlayTool(A1 050, slot=2)"`).
4. IF a future action type is needed THEN the enum SHALL be extensible without breaking existing switch/match logic (use a default/unknown arm or visitor pattern).

---

### Requirement 2: Expanded PokemonCard Data

**User Story:** As a developer, I want the `Card` struct (specifically the Pokémon variant) to carry `stage` data loaded from the database, so that placement legality can be checked at runtime. Evolution mechanics are out of scope for this iteration.

#### Acceptance Criteria

1. WHEN a `PokemonCard` is loaded from the database THEN the system SHALL populate:
   - `stage` (int: 0 = Basic, 1 = Stage 1, 2 = Stage 2)
2. IF a Pokémon has no stage field in the database THEN `stage` SHALL default to 0 (Basic).
3. WHEN the `Card` struct is printed or serialized THEN the `stage` field SHALL be included in the output.

---

### Requirement 3: Expanded TrainerCard Data

**User Story:** As a developer, I want the `Card` struct (specifically the Trainer variant) to carry a `TrainerType` sub-classification, so that the game engine can apply the correct rules for each trainer subtype.

#### Acceptance Criteria

1. WHEN the `TrainerType` enum is defined THEN the system SHALL include the following values:
   - `Item` — a one-shot card played from hand with an immediate effect; no per-turn limit
   - `Tool` — a card attached to a Pokémon in play that provides a persistent effect; one Tool per Pokémon at a time
   - `Supporter` — a card played from hand with a powerful effect; limited to one per turn
   - `Stadium` — a card placed in the shared Stadium zone that affects both players; replaces any existing Stadium
2. WHEN a `TrainerCard` is loaded from the database THEN the system SHALL populate its `trainer_type` field from the database record.
3. IF a card's trainer type is missing or unrecognized in the database THEN the system SHALL default to `Item` and log a warning.
4. WHEN the `Card` struct is printed or serialized THEN the `trainer_type` field SHALL be included in the output for Trainer cards.
5. WHEN a `TrainerCard` is stored in the `Card` struct THEN the `trainer_type` field SHALL be accessible without downcasting or type-unsafe operations.

---

### Requirement 4: Legal Move Generation

**User Story:** As a developer, I want a `generate_legal_moves(const GameState&, int player)` function that returns all valid `Action` values for the given player in the current game state, so that the game engine and AI can enumerate choices correctly.

#### Acceptance Criteria

1. WHEN `generate_legal_moves` is called THEN it SHALL return a `std::vector<Action>` containing every legal action for `player` given the current `GameState`.
2. WHEN the game is in the **Setup phase** (turn 1, no active Pokémon placed yet) THEN the system SHALL only generate `PlayPokemon` actions for Basic Pokémon (stage == 0) in hand targeting slot 0 (active).
3. WHEN the game is in the **Setup phase** and the active slot is already filled THEN the system SHALL generate `PlayPokemon` actions for empty bench slots (Basic Pokémon only) plus a `Pass` action.
4. WHEN the game is in the **Action phase** THEN the system SHALL generate:
   - `PlayPokemon` for each Basic Pokémon (stage == 0) in hand targeting any empty slot
   - `AttachEnergy` for each in-play Pokémon slot if the turn energy has not yet been attached
   - `PlaySupporter` for each Supporter card in hand (only if no Supporter has been played this turn)
   - `PlayItem` for each Item card in hand
   - `PlayTool` for each Tool card in hand targeting each in-play Pokémon slot that does not already have a Tool attached
   - `PlayStadium` for each Stadium card in hand
   - `Retreat` for each bench Pokémon if the active Pokémon has enough energy to pay the retreat cost and has not already retreated this turn
   - `Attack` for each attack of the active Pokémon that has sufficient energy attached
   - `Pass` always (player can always end their turn)
5. WHEN generating `Attack` actions THEN the system SHALL verify that the active Pokémon's attached energy satisfies the attack's `energy_required` list (matching both type and count, with Colorless matching any type).
6. WHEN generating `Retreat` actions THEN the system SHALL verify that the active Pokémon's attached energy covers the `retreat_cost` (same energy-matching rules as attacks).
7. IF the active slot is empty THEN the system SHALL NOT generate `Attack` or `Retreat` actions.
8. IF the turn energy has already been attached this turn THEN the system SHALL NOT generate additional `AttachEnergy` actions.
9. IF a Supporter card has already been played this turn THEN the system SHALL NOT generate `PlaySupporter` actions.
10. WHEN generating `PlayTool` actions THEN the system SHALL NOT generate a `PlayTool` action targeting a Pokémon slot that already has a Tool card attached.
11. WHEN generating `PlayStadium` actions THEN the system SHALL generate the action regardless of whether a Stadium is already in play (replacing it is always legal).

---

### Requirement 5: Minimum Rule Set for Legality

**User Story:** As a developer, I want a clearly defined minimum set of game rules enforced during move generation, so that the first implementation is correct and testable without requiring full rule coverage.

#### Acceptance Criteria

1. **Basic placement rule**: WHEN generating `PlayPokemon` THEN the system SHALL only allow Basic Pokémon (stage == 0) to be placed directly from hand; Stage 1 and Stage 2 Pokémon SHALL NOT be placeable directly. Evolution mechanics are deferred to a future iteration.
2. **One energy per turn rule**: WHEN the turn energy has been attached THEN the system SHALL track `energy_attached_this_turn` in `GameState` and block further `AttachEnergy` actions.
3. **One Supporter per turn rule**: WHEN a Supporter card has been played THEN the system SHALL track `supporter_played_this_turn` in `GameState` and block further `PlaySupporter` actions.
4. **One retreat per turn rule**: WHEN the active Pokémon has retreated THEN the system SHALL track `retreated_this_turn` in `GameState` and block further `Retreat` actions.
5. **Attack only once per turn**: WHEN an attack has been declared THEN the system SHALL transition to the Attack phase and no further `Attack` actions SHALL be generated (the turn ends after attack resolution).
6. **Energy generation starts on turn 2**: WHEN `turn_number == 1` THEN the system SHALL NOT generate any energy for either player; energy generation begins from turn 2 onward, meaning `AttachEnergy` actions SHALL NOT be generated on turn 1.
7. **One Tool per Pokémon rule**: WHEN a Tool card is already attached to a Pokémon slot THEN the system SHALL NOT generate a `PlayTool` action targeting that slot.
8. **Stadium replacement rule**: WHEN a `PlayStadium` action is applied THEN the system SHALL discard the previously active Stadium (if any) and place the new one; no per-turn limit applies.

---

### Requirement 6: GameState Turn Flag Tracking

**User Story:** As a developer, I want the `GameState` to track per-turn boolean flags needed for move generation legality, so that the rules in Requirement 5 can be enforced without scanning history.

#### Acceptance Criteria

1. WHEN `GameState` is extended THEN it SHALL add the following per-turn flags:
   - `energy_attached_this_turn` (bool, default false)
   - `supporter_played_this_turn` (bool, default false)
   - `retreated_this_turn` (bool, default false)
   - `attacked_this_turn` (bool, default false)
2. WHEN a new turn begins (phase transitions to Draw) THEN the system SHALL reset all four flags to false.
3. WHEN `AttachEnergy` is applied THEN `energy_attached_this_turn` SHALL be set to true.
4. WHEN a Supporter card is played THEN `supporter_played_this_turn` SHALL be set to true.
5. WHEN a Retreat action is applied THEN `retreated_this_turn` SHALL be set to true.
6. WHEN an Attack action is applied THEN `attacked_this_turn` SHALL be set to true.
7. WHEN `GameState` is extended THEN it SHALL add a `current_stadium` field (optional `CardId`) representing the Stadium currently in play; it SHALL be `nullopt` if no Stadium is active.
8. WHEN `PlayStadium` is applied THEN `current_stadium` SHALL be updated to the new Stadium's `CardId`.

---

### Requirement 7: CLI `--dump_moves` Subcommand

**User Story:** As a developer, I want a `ptcgp_cli util --dump_moves <deck1.json> <deck2.json>` command that initializes a game state, deals opening hands, and prints all legal moves for both players, so that I can manually verify the move generation logic.

#### Acceptance Criteria

1. WHEN `ptcgp_cli util --dump_moves <deck1.json> <deck2.json>` is invoked THEN the system SHALL:
   - Load both decks and initialize a `GameState` with shuffled decks and 5-card opening hands.
   - Print the full game state (turn number, phase, each player's hand, active/bench slots, current Stadium).
   - Call `generate_legal_moves` for the current player and print each action on a separate line.
2. WHEN the game state is printed THEN it SHALL include for each player: hand contents (card names + IDs + trainer subtype for Trainer cards), active Pokémon (name, HP, attached energy, attached Tool, status), and bench Pokémon (same fields).
3. WHEN legal moves are printed THEN each action SHALL be formatted as a numbered list with a human-readable description (e.g., `"1. PlayPokemon: Bulbasaur (A1 001) -> slot 0"`, `"3. PlayTool: Rocky Helmet (A1 088) -> slot 1"`).
4. IF no legal moves exist THEN the system SHALL print `"No legal moves available"`.
5. WHEN the command runs THEN it SHALL NOT crash on any valid deck input.
6. IF a deck file is missing or invalid THEN the system SHALL print an error and exit with code 1.

---

### Requirement 8: Unit Tests for Move Generation

**User Story:** As a developer, I want unit tests that verify `generate_legal_moves` produces correct output for known game states, so that regressions are caught automatically.

#### Acceptance Criteria

1. WHEN the test suite runs THEN it SHALL include a test that verifies: given a Setup-phase state with a Basic Pokémon in hand, `generate_legal_moves` returns exactly one `PlayPokemon` action targeting slot 0.
2. WHEN the test suite runs THEN it SHALL include a test that verifies: given an Action-phase state with the turn energy available and one Pokémon in play, `generate_legal_moves` includes an `AttachEnergy` action.
3. WHEN the test suite runs THEN it SHALL include a test that verifies: given an Action-phase state where the active Pokémon has sufficient energy for its attack, `generate_legal_moves` includes the corresponding `Attack` action.
4. WHEN the test suite runs THEN it SHALL include a test that verifies: given an Action-phase state where the active Pokémon does NOT have sufficient energy, `generate_legal_moves` does NOT include any `Attack` action.
5. WHEN the test suite runs THEN it SHALL include a test that verifies: `Pass` is always present in the legal moves during the Action phase.
6. WHEN the test suite runs THEN it SHALL include a test that verifies: on turn 1, no `AttachEnergy` actions are generated (energy generation starts on turn 2).
7. WHEN the test suite runs THEN it SHALL include a test that verifies: a `PlayTool` action is NOT generated for a Pokémon slot that already has a Tool attached.
8. WHEN the test suite runs THEN it SHALL include a test that verifies: a `PlaySupporter` action is NOT generated after a Supporter has already been played this turn.
9. WHEN the test suite runs THEN it SHALL include a test that verifies: a `PlayStadium` action IS generated even when a Stadium is already in play.
