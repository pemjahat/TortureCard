# Requirements Document

## Introduction

This feature adds two new debug-only CLI sub-commands — `--dump_supporter` and `--dump_item` — to the `util` command group. They scan the full card database, collect all **unique effect texts** for Supporter and Item trainer cards respectively, and write the results to JSON files. The output is intended purely for developer inspection (to discover what effect strings exist and plan mechanic implementations), mirroring the workflow of `--build_dictionary`. These commands must never be called at runtime during simulation.

---

## Requirements

### Requirement 1 — `--dump_supporter` CLI command

**User story:** As a developer, I want to run `ptcgp_cli util --dump_supporter` to get a JSON file listing every unique Supporter card effect text found in the database, so that I can plan and track which supporter mechanics still need to be implemented.

#### Acceptance Criteria

1. WHEN the user runs `ptcgp_cli util --dump_supporter` THEN the system SHALL load the card database from the default `database.json` path.
2. WHEN the database is loaded THEN the system SHALL iterate all cards, filter to those with `type == Trainer` and `trainer_type == Supporter`, and collect their effect text strings.
3. WHEN collecting effect texts THEN the system SHALL deduplicate them so each unique effect string appears exactly once in the output.
4. WHEN writing output THEN the system SHALL write a JSON file (default path: `database/supporter_mechanics.json`) whose top-level structure is an array of objects, each containing:
   - `"effect"`: the raw effect text string
   - `"cards"`: an array of card id strings (e.g. `"A1 219"`) that share this exact effect text
5. WHEN the output file cannot be opened for writing THEN the system SHALL print an error to `stderr` and return a non-zero exit code.
6. WHEN the command completes successfully THEN the system SHALL print a summary to `stdout` showing the total number of unique effect strings found and the output file path.
7. IF a Supporter card has no effect text in the database THEN the system SHALL include it in a separate `"no_effect"` array in the output JSON, listing its card id and name.

---

### Requirement 2 — `--dump_item` CLI command

**User story:** As a developer, I want to run `ptcgp_cli util --dump_item` to get a JSON file listing every unique Item card effect text found in the database, so that I can plan and track which item mechanics still need to be implemented.

#### Acceptance Criteria

1. WHEN the user runs `ptcgp_cli util --dump_item` THEN the system SHALL load the card database from the default `database.json` path.
2. WHEN the database is loaded THEN the system SHALL iterate all cards, filter to those with `type == Trainer` and `trainer_type == Item`, and collect their effect text strings.
3. WHEN collecting effect texts THEN the system SHALL deduplicate them so each unique effect string appears exactly once in the output.
4. WHEN writing output THEN the system SHALL write a JSON file (default path: `database/item_mechanics.json`) whose top-level structure is an array of objects, each containing:
   - `"effect"`: the raw effect text string
   - `"cards"`: an array of card id strings that share this exact effect text
5. WHEN the output file cannot be opened for writing THEN the system SHALL print an error to `stderr` and return a non-zero exit code.
6. WHEN the command completes successfully THEN the system SHALL print a summary to `stdout` showing the total number of unique effect strings found and the output file path.
7. IF an Item card has no effect text in the database THEN the system SHALL include it in a separate `"no_effect"` array in the output JSON, listing its card id and name.

---

### Requirement 3 — Trainer card effect text parsing

**User story:** As a developer, I want the `Card` struct to carry the raw effect text for trainer cards (Supporter, Item, Tool, Stadium), so that the dump commands can read it without re-parsing the raw JSON file.

#### Acceptance Criteria

1. WHEN a trainer card is parsed from `database.json` THEN the system SHALL read the card's `"effect"` field (if present) and store it in a new `std::optional<std::string> trainer_effect` field on the `Card` struct.
2. IF the `"effect"` field is absent or `null` for a trainer card THEN `trainer_effect` SHALL be `std::nullopt`.
3. WHEN the `Card` struct is used in existing simulation code THEN the new `trainer_effect` field SHALL have no impact on any existing logic (it is read-only metadata).

---

### Requirement 4 — CLI integration and help text

**User story:** As a developer, I want the new commands to be discoverable from the CLI help output, so that I know they exist.

#### Acceptance Criteria

1. WHEN `ptcgp_cli util` is run with no arguments THEN the help text SHALL include entries for `--dump_supporter` and `--dump_item` with a brief description.
2. WHEN `ptcgp_cli util --dump_supporter` or `ptcgp_cli util --dump_item` is invoked THEN the system SHALL NOT load or use any mechanic dictionary files (no `build_dictionaries`, no `pair_mechanic.json` dependency) — it reads only `database.json`.
3. WHEN the commands are invoked THEN they SHALL be implemented as static functions in `database.cpp` (or a new `database_debug.cpp`) following the same pattern as `build_attack_mechanic_dictionary`.
4. The dump functions SHALL NOT be called from any simulation code path (game loop, move generation, apply_action, etc.).
