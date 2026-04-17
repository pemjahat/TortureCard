# Requirements Document: Attack Effect Mechanic System

## Introduction

TortureCard currently resolves attacks using only a flat `damage` field on the `Attack` struct. Real PTCGP cards have rich attack effects described in natural-language text stored in `database.json` (e.g. "Flip 2 coins. This attack does 30 more damage for each heads."). This feature introduces a **Mechanic dictionary** — a compile-time map from effect text strings to typed `Mechanic` structs — so that `apply_attack_damage` can dispatch to the correct game logic instead of ignoring the effect text.

The design mirrors the approach used in `deckgym-core` (`effect_mechanic_map.rs` + `attacks/mechanic.rs`), adapted to C++17 and the existing TortureCard architecture.

To avoid re-parsing `database.json` on every run, the system generates two intermediate files:

1. **Mechanic Dictionary** (`database/mechanic_dictionary.json`) — maps each unique effect text string to its resolved `Mechanic` type and parameters.
2. **Attack Mechanic Dictionary** (`database/attack_mechanic_dictionary.json`) — maps each Pokémon card (by ID) and each of its attacks (by index) to the corresponding mechanic identifier, built on top of the mechanic dictionary.

Both files are generated automatically when absent and can be force-rebuilt via the CLI `--build_dictionary` flag.

---

## Requirements

### Requirement 1: Mechanic Enum / Variant Type

**User Story:** As a simulator developer, I want a strongly-typed `Mechanic` variant (C++ `std::variant` or tagged union) that captures every distinct attack mechanic and its parameters, so that the engine can dispatch to the correct logic without string parsing at runtime.

#### Acceptance Criteria

1. WHEN the codebase is compiled THEN `ptcgp_sim::Mechanic` SHALL be defined as a `std::variant` (or equivalent tagged struct) in `include/ptcgp_sim/mechanic.h`.
2. WHEN a mechanic has numeric parameters (e.g. coin count, damage per head) THEN those parameters SHALL be stored as named fields inside the variant alternative (e.g. `FlipNCoinDamage{ int coins; int heads_damage; int tails_damage; }`).
3. WHEN a mechanic has no parameters (e.g. `BasicDamage`) THEN it SHALL be represented as an empty struct alternative.
4. IF a `Mechanic` variant is compared for equality THEN the system SHALL support `operator==` so tests can assert exact mechanic values.
5. WHEN the initial set of mechanics is defined THEN it SHALL include at minimum:
   - `BasicDamage` — attack deals only its fixed `damage` field, no extra effect.
   - `FlipNCoinDamage` — flip N coins; total damage = `heads_damage × heads + tails_damage × tails` (replaces fixed damage entirely).
   - `FlipNCoinExtraDamage` — flip N coins; deal fixed damage + `extra_damage × heads`.
   - `SelfHeal` — heal a fixed amount from the attacking Pokémon after dealing damage.
   - `FlipUntilTailsDamage` — flip coins until tails; deal `damage_per_heads × heads`.

---

### Requirement 2: Attack struct carries an optional Mechanic

**User Story:** As a simulator developer, I want each `Attack` to optionally carry a resolved `Mechanic` so that the engine can use it during damage resolution without re-parsing text.

#### Acceptance Criteria

1. WHEN `Attack` is defined in `card.h` THEN it SHALL contain a field `std::optional<Mechanic> mechanic` (default `std::nullopt`).
2. WHEN an `Attack` has no effect text in `database.json` THEN its `mechanic` field SHALL be `std::nullopt` (treated as `BasicDamage` at resolution time).
3. WHEN an `Attack` has a recognised effect text THEN its `mechanic` field SHALL be populated with the corresponding `Mechanic` value after database loading.
4. WHEN an `Attack` has an unrecognised effect text THEN its `mechanic` field SHALL remain `std::nullopt` and a warning SHALL be emitted to `stderr` during database loading.

---

### Requirement 3: Effect-to-Mechanic Map (compile-time dictionary)

**User Story:** As a simulator developer, I want a static dictionary that maps raw effect text strings (from `database.json`) to `Mechanic` values, so that new mechanics can be added by inserting one entry without touching the resolution logic.

#### Acceptance Criteria

1. WHEN the library is initialised THEN `ptcgp_sim::effect_mechanic_map()` SHALL return a `const std::unordered_map<std::string_view, Mechanic>&` populated at program startup (e.g. via a `static` local or `inline` variable).
2. WHEN a new effect text is added to the map THEN no other source file SHALL need to be modified to make the new mechanic available during attack resolution.
3. WHEN the map is queried with an exact effect text string THEN it SHALL return the corresponding `Mechanic` in O(1) average time.
4. WHEN the map is queried with an unknown string THEN it SHALL return `std::nullopt` (or an empty `std::optional<Mechanic>`).
5. WHEN the initial map is populated THEN it SHALL cover at minimum all coin-flip damage variants present in `database.json`:
   - `"Flip 1 coin. This attack does X damage."` → `FlipNCoinDamage{coins:1, heads_damage:X, tails_damage:0}`
   - `"Flip N coins. This attack does X damage for each heads."` → `FlipNCoinExtraDamage{coins:N, extra_damage:X}` (with `include_fixed_damage: false`)
   - `"Flip N coins. This attack does X more damage for each heads."` → `FlipNCoinExtraDamage{coins:N, extra_damage:X}` (with `include_fixed_damage: true`)
   - `"Flip a coin until you get tails. This attack does X damage for each heads."` → `FlipUntilTailsDamage{damage_per_heads:X}`
   - `"Heal X damage from this Pokémon."` → `SelfHeal{amount:X}`

---

### Requirement 4: Database loading populates Attack::mechanic

**User Story:** As a simulator developer, I want the `Database::load()` function to resolve each attack's effect text against the mechanic map and store the result in `Attack::mechanic`, so that loaded cards are immediately ready for simulation.

#### Acceptance Criteria

1. WHEN `Database::load()` parses a Pokémon card THEN for each `Attack` it SHALL look up `attack.effect` in `effect_mechanic_map()`.
2. IF the effect text is found in the map THEN `Attack::mechanic` SHALL be set to the corresponding `Mechanic`.
3. IF the effect text is `null` / absent in JSON THEN `Attack::mechanic` SHALL remain `std::nullopt`.
4. IF the effect text is non-null but not found in the map THEN `Attack::mechanic` SHALL remain `std::nullopt` AND a single `stderr` warning line SHALL be printed: `[warn] unknown attack effect: "<text>"`.
5. WHEN the database is loaded in tests THEN the loaded `Attack::mechanic` values SHALL match the expected `Mechanic` variants for known cards (e.g. Rattata's "Gnaw" with a coin-flip effect).

---

### Requirement 5: apply_attack_damage dispatches on Mechanic

**User Story:** As a simulator developer, I want `apply_attack_damage` to use `Attack::mechanic` when present so that coin-flip attacks, self-heal attacks, and other non-trivial effects are resolved correctly during simulation.

#### Acceptance Criteria

1. WHEN `apply_attack_damage` is called and `attack.mechanic` is `std::nullopt` THEN it SHALL apply only the fixed `attack.damage` (existing behaviour, no regression).
2. WHEN `attack.mechanic` is `FlipNCoinDamage{coins, heads_damage, tails_damage}` THEN the system SHALL flip `coins` coins using the provided `std::mt19937 rng`, compute `total = heads_damage × heads + tails_damage × tails`, and deal `total` damage (ignoring `attack.damage`).
3. WHEN `attack.mechanic` is `FlipNCoinExtraDamage{coins, extra_damage, include_fixed_damage}` THEN the system SHALL flip `coins` coins, compute `extra = extra_damage × heads`, and deal `(include_fixed_damage ? attack.damage : 0) + extra` damage.
4. WHEN `attack.mechanic` is `SelfHeal{amount}` THEN after dealing `attack.damage` the system SHALL reduce the attacker's `damage_counters` by `amount` (clamped to 0).
5. WHEN `attack.mechanic` is `FlipUntilTailsDamage{damage_per_heads}` THEN the system SHALL flip coins until tails, count heads, and deal `damage_per_heads × heads` damage.
6. WHEN weakness is applied THEN it SHALL be applied to the final computed damage (after mechanic resolution), consistent with existing behaviour.
7. WHEN a mechanic variant is not yet handled in the dispatch THEN the system SHALL fall back to `attack.damage` and emit a `stderr` warning rather than crashing.

---

### Requirement 6: Unit tests for Mechanic dispatch

**User Story:** As a simulator developer, I want unit tests that verify each implemented `Mechanic` variant produces the correct game-state outcome, so that regressions are caught automatically.

#### Acceptance Criteria

1. WHEN the test suite is built THEN a new test target `ptcgp_test_attack_effects` SHALL be added to `CMakeLists.txt`.
2. WHEN `FlipNCoinDamage{coins:1, heads_damage:30, tails_damage:0}` is resolved with a seeded RNG that produces heads THEN `gs.players[1].pokemon_slots[0]->damage_counters` SHALL equal 30.
3. WHEN `FlipNCoinDamage{coins:1, heads_damage:30, tails_damage:0}` is resolved with a seeded RNG that produces tails THEN `damage_counters` SHALL equal 0.
4. WHEN `FlipNCoinExtraDamage{coins:2, extra_damage:20, include_fixed_damage:true}` is resolved with 2 heads (seeded RNG) and `attack.damage = 40` THEN `damage_counters` SHALL equal 80.
5. WHEN `SelfHeal{amount:30}` is resolved and the attacker has 50 `damage_counters` THEN after the attack the attacker's `damage_counters` SHALL equal 20.
6. WHEN `FlipUntilTailsDamage{damage_per_heads:20}` is resolved with a seeded RNG producing H, H, T THEN `damage_counters` SHALL equal 40.

---

### Requirement 7: Intermediate Mechanic Dictionary File

**User Story:** As a simulator developer, I want the system to compile `database.json` into a local intermediate dictionary file so that subsequent runs load the pre-resolved mechanic mapping instantly without re-parsing the full JSON, and I can force a rebuild via a CLI flag when the source data changes.

#### Acceptance Criteria

1. WHEN `Database::load()` is called THEN the library SHALL check whether the intermediate dictionary file (e.g. `database/mechanic_dictionary.json`) exists on disk.
2. IF the intermediate dictionary file does not exist THEN the library SHALL automatically build it by parsing `database.json`, resolving each attack's effect text against `effect_mechanic_map()`, and writing the result to the intermediate file before proceeding.
3. IF the intermediate dictionary file already exists THEN the library SHALL load the pre-resolved mechanic mapping directly from it, skipping full JSON re-parsing of attack effects.
4. WHEN the intermediate file is written THEN its format SHALL be a human-readable JSON mapping of `{ "effect text": { "mechanic_type": "...", ...params } }` so developers can inspect and debug it.
5. WHEN the intermediate file is loaded THEN the resulting in-memory map SHALL be identical to what would have been produced by a fresh parse of `database.json`.
6. WHEN the CLI is invoked with the `--build_dictionary` flag THEN `ptcgp_cli` SHALL regenerate the intermediate dictionary file unconditionally (overwriting any existing file) and print a confirmation message to `stdout`, then exit.
7. WHEN `--build_dictionary` is run THEN it SHALL report to `stdout` the count of recognised effects, the count of unrecognised effects (those not in `effect_mechanic_map()`), and the output file path.
8. WHEN the intermediate file is absent or `--build_dictionary` is used THEN the library/CLI SHALL emit a `stdout` status line: `[info] Building mechanic dictionary from database.json...`.
9. IF the intermediate file cannot be written (e.g. permission error) THEN the library SHALL fall back to in-memory resolution and emit a `stderr` warning: `[warn] Could not write mechanic dictionary: <reason>`.

---

### Requirement 8: Attack Mechanic Dictionary File

**User Story:** As a simulator developer, I want the system to build a second intermediate file that maps every Pokémon card's attacks to their resolved mechanic identifiers, so that the engine can look up a card's attack mechanic by Pokémon ID and attack index in O(1) without re-resolving effect text at runtime.

#### Acceptance Criteria

1. WHEN the mechanic dictionary (Requirement 7) has been built or loaded THEN the library SHALL next check whether the attack mechanic dictionary file (e.g. `database/attack_mechanic_dictionary.json`) exists on disk.
2. IF the attack mechanic dictionary file does not exist THEN the library SHALL build it by iterating every Pokémon card in `database.json`, and for each attack resolving its effect text via the mechanic dictionary, then writing the result to the file.
3. IF the attack mechanic dictionary file already exists THEN the library SHALL load it directly, skipping re-resolution.
4. WHEN the attack mechanic dictionary file is written THEN its format SHALL be a human-readable JSON structured as:
   ```json
   {
     "<pokemon_id>": {
       "attacks": [
         { "attack_index": 0, "mechanic_type": "BasicDamage", "params": {} },
         { "attack_index": 1, "mechanic_type": "FlipNCoinDamage", "params": { "coins": 1, "heads_damage": 30, "tails_damage": 0 } }
       ]
     }
   }
   ```
   where `<pokemon_id>` is the card's full ID string (e.g. `"A1 001"`).
5. WHEN an attack has no effect text (null in JSON) THEN its entry SHALL record `"mechanic_type": "BasicDamage"` with empty `"params": {}`.
6. WHEN an attack's effect text is not found in the mechanic dictionary THEN its entry SHALL record `"mechanic_type": "Unknown"` with `"params": {}`, and a `stderr` warning SHALL be emitted.
7. WHEN `Database::load()` reads the attack mechanic dictionary THEN it SHALL apply the stored mechanic to each corresponding `Attack::mechanic` field on the loaded `Card` objects, so cards are fully resolved without touching `effect_mechanic_map()` at load time.
8. WHEN the CLI `--build_dictionary` flag is used THEN it SHALL rebuild both the mechanic dictionary (Requirement 7) and the attack mechanic dictionary in sequence, reporting combined stats: total Pokémon processed, total attacks mapped, and counts of `BasicDamage` / resolved / unknown entries.
9. WHEN the attack mechanic dictionary file cannot be written THEN the library SHALL fall back to in-memory resolution and emit a `stderr` warning: `[warn] Could not write attack mechanic dictionary: <reason>`.
10. WHEN the attack mechanic dictionary is loaded THEN the resulting per-card mechanic assignments SHALL be identical to what would have been produced by a fresh resolution pass through `effect_mechanic_map()`.
