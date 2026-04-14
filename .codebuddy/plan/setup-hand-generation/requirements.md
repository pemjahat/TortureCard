# Requirements Document: Valid Hand Generation for Setup Phase

## Introduction

In the Pokemon TCG Pocket (PTCGP) rules, before the game begins each player draws a starting hand of 5 cards from their shuffled deck. A valid starting hand **must contain at least one Basic Pokémon (stage 0)**. If a player's initial draw contains no Basic Pokémon, the deck must be reshuffled and redrawn until a valid hand is obtained.

Currently in the TortureCard (`ptcgp_sim`) C++ simulator, the `--dump_moves` command in `main.cpp` manually pops 5 cards off the back of the unshuffled deck without any shuffle or validity check. The `GameState::make()` factory also leaves both players' hands empty and decks unshuffled. There is no dedicated function to perform the initial deal with the valid-hand guarantee.

This feature implements the correct Setup-phase hand generation: shuffle each player's deck and deal 5 cards, guaranteeing at least one stage-0 Pokémon in the starting hand (reshuffling and redealing as needed), mirroring the logic in the reference Rust implementation (`deckgym-core`'s `Deck::shuffle(initial_shuffle=true, ...)`).

The implementation lives in the `ptcgp_sim` C++ library (`src/lib/`) and is exposed through the `Deck` struct and/or `GameState`.

---

## Requirements

### Requirement 1 — Deck Shuffle

**User Story:** As a game engine developer, I want the deck to be shuffled randomly before the game starts, so that each game begins with a different card order.

#### Acceptance Criteria

1. WHEN `Deck::shuffle(rng)` is called THEN the system SHALL randomly reorder all cards in `Deck::cards` using the provided random number generator.
2. WHEN `Deck::shuffle(rng)` is called THEN the system SHALL NOT alter the `energy_types` or `entries` fields of the deck.
3. WHEN `Deck::shuffle(rng)` is called with a fixed seed THEN the system SHALL produce a deterministic card order for that seed.

---

### Requirement 2 — Valid Starting Hand Deal

**User Story:** As a game engine developer, I want to deal each player a starting hand of exactly 5 cards that contains at least one Basic Pokémon (stage 0), so that the Setup phase can always proceed with a legal active slot placement.

#### Acceptance Criteria

1. WHEN `Deck::deal_starting_hand(rng)` is called THEN the system SHALL return a `std::vector<Card>` of exactly 5 cards drawn from the top of the deck, and remove those 5 cards from `Deck::cards`.
2. WHEN the 5 drawn cards contain no stage-0 Pokémon THEN the system SHALL return all 5 cards to the deck, reshuffle, and redraw until at least one stage-0 Pokémon is present.
3. WHEN `deal_starting_hand` guarantees a valid hand THEN the system SHALL ensure the first card in the returned vector is a stage-0 Pokémon (to mirror the deckgym-core convention of placing a Basic at index 0).
4. IF the deck contains no stage-0 Pokémon at all THEN the system SHALL assert/abort with a clear error message (this is a deck-validation concern; a valid deck always has at least one Basic Pokémon).
5. WHEN `deal_starting_hand` completes THEN `Deck::cards` SHALL contain exactly `(original_size - 5)` cards.

---

### Requirement 3 — GameState Initialization with Valid Hands

**User Story:** As a game engine developer, I want `GameState::make()` (or a new `GameState::deal_starting_hands(rng)` helper) to automatically shuffle both decks and deal valid starting hands to both players, so that a freshly created `GameState` is ready for the Setup phase without any manual card manipulation.

#### Acceptance Criteria

1. WHEN `GameState::deal_starting_hands(rng)` is called on a `GameState` whose players have empty hands THEN the system SHALL shuffle each player's deck and deal each player a valid 5-card starting hand using `Deck::deal_starting_hand(rng)`.
2. AFTER `deal_starting_hands` completes THEN each `PlayerState::hand` SHALL contain exactly 5 cards with at least one stage-0 Pokémon.
3. AFTER `deal_starting_hands` completes THEN each `PlayerState::deck.cards` SHALL contain exactly 15 cards (20 total − 5 dealt).
4. WHEN `deal_starting_hands` is called THEN the system SHALL NOT modify `pokemon_slots`, `discard_pile`, `points`, or any other `GameState` fields beyond the two players' `hand` and `deck`.
5. WHEN `deal_starting_hands` is called on a `GameState` that already has non-empty hands THEN the system SHALL assert/abort to prevent double-dealing.

---

### Requirement 4 — CLI Integration

**User Story:** As a developer using the CLI tool, I want the `--dump_moves` and `--simulate_turn` commands to use the new valid hand generation, so that the printed game state reflects a realistic starting position.

#### Acceptance Criteria

1. WHEN `ptcgp_cli util --dump_moves <deck1> <deck2>` is run THEN the system SHALL call `deal_starting_hands` (with a random or seeded RNG) instead of manually popping 5 cards from the unshuffled deck.
2. WHEN `ptcgp_cli util --simulate_turn <deck1> <deck2>` is run THEN the system SHALL call `deal_starting_hands` so that the printed hand sizes and deck sizes are correct (5 cards in hand, 15 in deck).
3. WHEN the CLI prints the game state THEN each player's hand SHALL contain at least one stage-0 Pokémon.

---

### Requirement 5 — Unit Tests

**User Story:** As a developer, I want unit tests that verify the valid hand generation logic, so that regressions are caught automatically.

#### Acceptance Criteria

1. WHEN a test calls `deal_starting_hand` on a deck where all Pokémon are stage 0 THEN the system SHALL return a hand containing at least one stage-0 Pokémon.
2. WHEN a test calls `deal_starting_hand` on a deck where the first 5 cards are all non-Pokémon (Trainers) but the deck contains stage-0 Pokémon elsewhere THEN the system SHALL still return a hand with at least one stage-0 Pokémon (verifying the reshuffle-and-redraw loop).
3. WHEN a test calls `deal_starting_hand` with a fixed RNG seed THEN the system SHALL return the same hand deterministically.
4. WHEN a test calls `GameState::deal_starting_hands` THEN both players SHALL have exactly 5 cards in hand and 15 cards remaining in their decks.
5. WHEN a test calls `GameState::deal_starting_hands` THEN both players' hands SHALL each contain at least one stage-0 Pokémon.
6. WHEN a test calls `Deck::shuffle` THEN the deck size SHALL remain unchanged.
