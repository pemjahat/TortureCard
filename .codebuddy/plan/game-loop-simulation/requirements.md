# Requirements Document: Full Game Loop Simulation with AttachAttack Player

## Introduction

This feature implements a complete, playable game loop for the TortureCard PTCG Pocket simulator. It introduces:

1. **A Player decision interface** — an abstract `Player` class (inspired by `deckgym-core`'s `Player` trait) that receives the current `GameState` and a list of legal moves, and returns one chosen `Action`.
2. **An `AttachAttackPlayer` strategy** — a concrete player that always prefers to attach energy to the active Pokémon (prioritizing the minimum energy needed for an attack), and attacks if possible; otherwise it falls back to the first available legal move.
3. **A full game loop** — drives `GameState` through all turn phases (Setup → Draw → Action → Attack → Cleanup) for both players until one player reaches 3+ points, with proper energy generation each turn.
4. **A `--simulate` CLI sub-command** — replaces the stub `sim` command in `main.cpp`, accepting two deck JSON files and running a single game to completion, printing a turn-by-turn log and the final result.

### Scope

- Both players use the same `AttachAttackPlayer` strategy.
- The game loop handles: setup phase (place active + bench), draw phase (draw one card per turn), action phase (player decisions), attack phase (attack if declared), and cleanup phase (status damage, KO resolution, active promotion).
- Active Pokémon promotion after a KO is handled automatically: the first available bench Pokémon is promoted.
- Energy generation: each turn (starting turn 2) a random energy matching the current player's deck energy type is generated and made available.
- The game ends when a player reaches 3 points or has no Pokémon left to promote.
- Turn limit of 200 turns is enforced to prevent infinite loops.

---

## Requirements

### Requirement 1: Player Decision Interface

**User Story:** As a developer, I want a `Player` abstract interface so that different AI strategies can be plugged into the game loop without changing the loop itself.

#### Acceptance Criteria

1. WHEN the system is compiled THEN a `Player` abstract base class SHALL exist in `include/ptcgp_sim/player.h` with a pure virtual method `decide(const GameState& gs, const std::vector<Action>& legal_moves) -> Action`.
2. WHEN `decide` is called THEN the returned `Action` SHALL be one of the actions present in the `legal_moves` vector.
3. WHEN a concrete player is constructed THEN it SHALL be usable through a `Player*` or `std::unique_ptr<Player>` pointer.

---

### Requirement 2: AttachAttackPlayer Strategy

**User Story:** As a developer, I want a simple `AttachAttackPlayer` that mirrors the deckgym-core reference implementation, so that the game loop can run without requiring complex AI.

#### Acceptance Criteria

1. WHEN `decide` is called with a list of legal moves THEN the `AttachAttackPlayer` SHALL first look for an `AttachEnergy` action targeting the active Pokémon (slot 0).
2. IF an `AttachEnergy` action to slot 0 is available THEN the player SHALL return it.
3. IF no `AttachEnergy` to slot 0 is available AND an `Attack` action is available THEN the player SHALL return the first available `Attack` action.
4. IF neither `AttachEnergy` to slot 0 nor `Attack` is available THEN the player SHALL return the first action in the `legal_moves` list (fallback).
5. WHEN the active Pokémon already has enough energy for its cheapest attack THEN the player SHALL prefer `Attack` over further energy attachment to slot 0.

---

### Requirement 3: Turn Phase Driver (Game Loop Engine)

**User Story:** As a developer, I want a `GameLoop` class (or free function) that drives `GameState` through all turn phases automatically, so that a complete game can be played from start to finish.

#### Acceptance Criteria

1. WHEN a game starts THEN the loop SHALL begin in `TurnPhase::Setup` and call `deal_starting_hands` before the first turn.
2. WHEN `TurnPhase::Setup` is active THEN the loop SHALL ask the current player to place their active Pokémon (and optionally bench Pokémon) by calling `decide` until the player passes.
3. WHEN `TurnPhase::Draw` is active THEN the loop SHALL draw one card from the current player's deck into their hand (if the deck is non-empty), then advance to `TurnPhase::Action`.
4. WHEN `TurnPhase::Action` is active THEN the loop SHALL repeatedly call `decide` on the current player and apply the chosen action until the player passes or attacks.
5. WHEN `TurnPhase::Action` is active AND `current_energy` is `nullopt` AND `turn_number >= 2` THEN the loop SHALL generate a random energy matching the current player's deck energy type and set `gs.current_energy` before asking for decisions.
6. WHEN `TurnPhase::Attack` is active THEN the loop SHALL advance to `TurnPhase::Cleanup` (attack was already applied during Action phase).
7. WHEN `TurnPhase::Cleanup` is active THEN the loop SHALL apply end-of-turn status damage (Poison: 10 damage, Burn: 10 damage), resolve any resulting KOs, then advance to the next player's turn.
8. WHEN a Pokémon is knocked out and the owner has bench Pokémon THEN the loop SHALL automatically promote the first available bench Pokémon to the active slot.
9. WHEN `gs.game_over` is true THEN the loop SHALL stop immediately and return the `SimulationResult`.
10. WHEN 200 turns have elapsed without a winner THEN the loop SHALL set `game_over = true`, `winner = -1` (draw), and stop.

---

### Requirement 4: Energy Generation

**User Story:** As a developer, I want the game loop to generate one energy per turn for the current player, so that Pokémon can accumulate energy and eventually attack.

#### Acceptance Criteria

1. WHEN a new turn begins (turn_number >= 2) THEN the loop SHALL generate exactly one energy of the type matching the current player's deck `energy_types[0]`.
2. WHEN the current player's deck has multiple energy types THEN the loop SHALL randomly select one of them using the game's RNG.
3. WHEN `gs.current_energy` is already set (not `nullopt`) at the start of a turn THEN the loop SHALL NOT overwrite it.
4. WHEN `gs.reset_turn_flags()` is called THEN `current_energy` SHALL be reset to `nullopt` (already handled by existing code).

---

### Requirement 5: CLI `--simulate` Sub-command

**User Story:** As a user, I want to run `ptcgp_cli sim <deck1.json> <deck2.json>` to play a full game and see the result, so that I can verify the simulator works end-to-end.

#### Acceptance Criteria

1. WHEN the user runs `ptcgp_cli sim <deck1.json> <deck2.json>` THEN the CLI SHALL load both decks, validate them, initialize a `GameState`, and run the full game loop to completion.
2. WHEN the game completes THEN the CLI SHALL print the winner (Player 0 or Player 1), the total number of turns played, and each player's final point score.
3. WHEN a deck fails validation THEN the CLI SHALL print an error and exit with code 1.
4. WHEN the game ends in a draw (turn limit reached) THEN the CLI SHALL print "Draw (turn limit reached)" and exit with code 0.
5. WHEN the `--verbose` flag is provided THEN the CLI SHALL print a turn-by-turn log showing: turn number, current player, action chosen, and game state summary after each action.
6. WHEN the game loop runs THEN the CLI SHALL use a seeded RNG (default seed = `std::random_device{}()`) so results are reproducible when a seed is provided via `--seed <N>`.

---

### Requirement 6: Active Pokémon Promotion After KO

**User Story:** As a developer, I want the game loop to automatically promote a bench Pokémon when the active is knocked out, so that the game can continue without manual intervention.

#### Acceptance Criteria

1. WHEN the active Pokémon (slot 0) is knocked out THEN the loop SHALL scan bench slots 1–3 in order and promote the first occupied slot to slot 0.
2. WHEN the active is knocked out AND no bench Pokémon remain THEN the loop SHALL set `gs.game_over = true` and award the win to the opponent (already handled by `resolve_knockouts`).
3. WHEN a bench Pokémon is promoted to active THEN its `played_this_turn` flag SHALL be set to `false` so it can evolve on the next turn.
4. WHEN promotion occurs THEN the promoted Pokémon's volatile status conditions SHALL NOT be cleared (promotion is not a retreat).

---

### Requirement 7: Simulator Integration

**User Story:** As a developer, I want the existing `Simulator::run` stub to be implemented using the new game loop, so that `run_batch` works correctly for future batch simulations.

#### Acceptance Criteria

1. WHEN `Simulator::run(deck_p0, deck_p1)` is called THEN it SHALL create two `AttachAttackPlayer` instances, initialize a `GameState`, and run the full game loop.
2. WHEN the game completes THEN `Simulator::run` SHALL return a `SimulationResult` with the correct `winner` (0, 1, or -1) and `turns` count.
3. WHEN `Simulator::run_batch` is called THEN it SHALL call `Simulator::run` for each game and accumulate win counts correctly.
