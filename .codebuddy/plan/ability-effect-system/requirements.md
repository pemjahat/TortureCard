# Requirements Document: Ability Effect System

## Introduction

This feature introduces a **Pokémon Ability Effect System** into the TortureCard C++ simulator, modeled after the existing attack-mechanic dictionary pattern already in place. As part of this work, the existing attack-mechanic types and dictionaries are **renamed** for consistency, and a parallel ability-mechanic system is added alongside them.

### Naming Conventions (New vs. Old)

| Old Name | New Name | Notes |
|---|---|---|
| `Mechanic` (base struct) | `AttackMechanic` | Renamed for clarity |
| `BasicDamage`, `SelfHeal`, etc. | Same names, now subtypes of `AttackMechanic` | No logic change |
| `effect_mechanic_map()` | `attack_mechanic_dictionary()` | Renamed function |
| `mechanic_map.h/.cpp` | `attack_mechanic_dictionary.h/.cpp` | Renamed files |
| *(new)* | `AbilityMechanic` | New base struct for ability effects |
| *(new)* | `ability_mechanic_dictionary()` | New function, maps effect text → `AbilityMechanic` |
| *(new)* | `pair_attack_mechanic()` | Maps `CardId` → `AttackMechanic*` per attack |
| *(new)* | `pair_ability_mechanic()` | Maps `CardId` → `AbilityMechanic*` |

### Key Design Question: Can Attack and Ability Mechanic Dictionaries Be Combined?

**No — they must remain separate.** The reasons are:

1. **Different resolution contexts**: `AttackMechanic` resolves during `apply_attack_damage`. `AbilityMechanic` resolves at multiple distinct checkpoints (action phase, damage phase, end-of-turn, retreat, etc.).
2. **Different interfaces**: `AttackMechanic` implements `compute_damage` + `apply_post_damage`. `AbilityMechanic` modifies game state through hooks with no damage computation role.
3. **Disjoint key namespaces**: Attack effect text and ability effect text are separate fields in the card database.

The two dictionaries share the same **structural pattern** (effect text → prototype object) but are distinct types and files.

### First-Pass Scope

This first implementation pass is intentionally limited:

- **Activate abilities implemented**: `HealAllYourPokemon`, `HealOneYourPokemon`, `HealActiveYourPokemon` (the "Heal" family)
- **Passive abilities implemented**: `ReduceDamageFromAttacks` (the "ReduceDamage" family)
- **All other ability effects**: Fall through to `UnknownAbilityMechanic` — no crash, no effect, to be implemented in future passes

### Ability Timing Classification

| Timing | Description | First-Pass Examples |
|---|---|---|
| **Activate** | Player-triggered, once per turn, during Action phase | Heal all / one / active Pokémon |
| **Passive** | Always-on or event-triggered, no player action required | Reduce incoming damage |

---

## Requirements

### Requirement 1: Rename `Mechanic` → `AttackMechanic`

**User Story:** As a simulator developer, I want the existing `Mechanic` base struct and all its subtypes, files, and references renamed to `AttackMechanic`, so that the codebase clearly distinguishes attack mechanics from ability mechanics.

#### Acceptance Criteria

1. WHEN the rename is applied THEN the header `include/ptcgp_sim/mechanic.h` SHALL be renamed to `include/ptcgp_sim/attack_mechanic.h` and the base struct `Mechanic` SHALL become `AttackMechanic`
2. WHEN the rename is applied THEN all concrete subtypes (`BasicDamage`, `FlipNCoinDamage`, `FlipNCoinExtraDamage`, `SelfHeal`, `FlipUntilTailsDamage`, `UnknownMechanic`) SHALL remain functionally identical but inherit from `AttackMechanic`
3. WHEN the rename is applied THEN `src/lib/mechanic.cpp` SHALL be renamed to `src/lib/attack_mechanic.cpp` with all internal references updated
4. WHEN the rename is applied THEN `include/ptcgp_sim/mechanic_map.h` SHALL be renamed to `include/ptcgp_sim/attack_mechanic_dictionary.h` and the function `effect_mechanic_map()` SHALL be renamed to `attack_mechanic_dictionary()`
5. WHEN the rename is applied THEN `src/lib/mechanic_map.cpp` SHALL be renamed to `src/lib/attack_mechanic_dictionary.cpp` with all internal references updated
6. WHEN the rename is applied THEN all files that `#include "ptcgp_sim/mechanic.h"` or `"ptcgp_sim/mechanic_map.h"` SHALL be updated to use the new header names
7. WHEN the rename is applied THEN the `Attack` struct in `card.h` SHALL update its `mechanic` field type from `unique_ptr<Mechanic>` to `unique_ptr<AttackMechanic>`
8. WHEN all renames are applied THEN the project SHALL compile and all existing tests SHALL pass without modification to test logic

### Requirement 2: Add `pair_attack_mechanic` — CardId-to-AttackMechanic Mapping

**User Story:** As a simulator developer, I want a `pair_attack_mechanic()` function that maps each Pokémon `CardId` + attack index to its resolved `AttackMechanic`, so that attack mechanic lookup during gameplay can be done by card identity rather than by re-parsing effect text.

#### Acceptance Criteria

1. WHEN the system is compiled THEN `attack_mechanic_dictionary.h` SHALL declare a function `pair_attack_mechanic()` returning a `const unordered_map<string, const AttackMechanic*>&` keyed by a composite string of `CardId::to_string() + ":" + attack_index`
2. WHEN `pair_attack_mechanic()` is called THEN it SHALL return a static singleton map built by iterating over all known Pokémon cards and looking up each attack's effect text in `attack_mechanic_dictionary()`
3. WHEN a key is looked up in the map THEN it SHALL return a non-owning pointer to the prototype `AttackMechanic` if the attack has a known mechanic, or `nullptr` if not
4. WHEN an attack's effect text is not in `attack_mechanic_dictionary()` THEN the lookup SHALL return `nullptr` (graceful degradation, no crash)

### Requirement 3: `AbilityMechanic` Base Type

**User Story:** As a simulator developer, I want a well-typed `AbilityMechanic` abstract base struct (parallel to `AttackMechanic`) so that each ability effect can be represented as a concrete subtype with its parameters, enabling type-safe dispatch and equality comparison.

#### Acceptance Criteria

1. WHEN the system is compiled THEN a new header `include/ptcgp_sim/ability_mechanic.h` SHALL define an abstract base struct `AbilityMechanic` with:
   - A `timing()` method returning an `AbilityTiming` enum (`Activate` or `Passive`)
   - A `passive_hook()` method returning a `PassiveHook` enum (meaningful only for passive abilities; values: `DamagePhase`, `EndOfTurn`, `RetreatCost`, `GameState`, `OnEvolve`, `OnBenchPlay`)
   - `equals()`, `clone()`, `type_name()`, `params_json()`, `from_params_json()` methods (mirroring `AttackMechanic`)
2. WHEN a concrete `AbilityMechanic` subtype is defined THEN it SHALL implement all pure virtual methods
3. WHEN two `AbilityMechanic` instances of the same concrete type with the same parameters are compared THEN `equals()` SHALL return `true`
4. WHEN two `AbilityMechanic` instances of different concrete types are compared THEN `equals()` SHALL return `false`
5. WHEN `clone()` is called on any concrete `AbilityMechanic` THEN it SHALL return a deep copy as a `unique_ptr<AbilityMechanic>`

### Requirement 4: Concrete `AbilityMechanic` Subtypes — First Pass

**User Story:** As a simulator developer, I want the first-pass set of concrete `AbilityMechanic` subtypes (Heal activate + ReduceDamage passive) implemented, with all other ability effects falling into `UnknownAbilityMechanic`, so that the system is functional and extensible without requiring all abilities upfront.

#### Acceptance Criteria

1. WHEN the system is compiled THEN the following **Activate** subtypes SHALL be defined and fully implemented:
   - `HealAllYourPokemon { amount: int }` — heals all of the player's in-play Pokémon by `amount`; `timing()` returns `Activate`
   - `HealOneYourPokemon { amount: int }` — heals one chosen in-play Pokémon by `amount`; `timing()` returns `Activate`
   - `HealActiveYourPokemon { amount: int }` — heals the player's active Pokémon by `amount`; `timing()` returns `Activate`

2. WHEN the system is compiled THEN the following **Passive** subtype SHALL be defined and fully implemented:
   - `ReduceDamageFromAttacks { amount: int }` — reduces incoming attack damage to this Pokémon by `amount` (minimum 0); `timing()` returns `Passive`; `passive_hook()` returns `DamagePhase`

3. WHEN the system is compiled THEN an `UnknownAbilityMechanic` subtype SHALL be defined as a no-op fallback:
   - `timing()` returns `Passive` (safe default — will not be player-triggered)
   - `passive_hook()` returns `DamagePhase` (safe default — no-op in damage pipeline)
   - All game-state methods are no-ops; no crash occurs

4. WHEN a concrete subtype has parameters THEN those parameters SHALL be stored as struct fields and serialized via `params_json()` / `from_params_json()`

### Requirement 5: `ability_mechanic_dictionary` — Effect Text to AbilityMechanic

**User Story:** As a simulator developer, I want a static `ability_mechanic_dictionary()` function mapping raw ability effect text to prototype `AbilityMechanic` instances, so that ability effects can be resolved by text lookup — mirroring the `attack_mechanic_dictionary()` pattern.

#### Acceptance Criteria

1. WHEN the system is compiled THEN a new header `include/ptcgp_sim/ability_mechanic_dictionary.h` SHALL declare a function `ability_mechanic_dictionary()` returning a `const unordered_map<string, unique_ptr<AbilityMechanic>>&`
2. WHEN `ability_mechanic_dictionary()` is called THEN it SHALL return a static singleton map initialized once at first call
3. WHEN an ability effect text string is looked up in the map THEN the map SHALL return the corresponding prototype `AbilityMechanic` if registered, or indicate absence (via `map.count() == 0`) if not
4. WHEN a new ability effect is added to `ability_mechanic_dictionary.cpp` THEN no other file SHALL need to be modified to support the new lookup
5. WHEN the map is initialized THEN it SHALL contain entries for all Heal-family activate effects and all ReduceDamage-family passive effects corresponding to the subtypes in Requirement 4
6. WHEN an ability effect text is not in the map THEN callers SHALL treat the absence as `UnknownAbilityMechanic` (no crash)

### Requirement 6: `pair_ability_mechanic` — CardId-to-AbilityMechanic Mapping

**User Story:** As a simulator developer, I want a `pair_ability_mechanic()` function that maps each Pokémon `CardId` to its resolved `AbilityMechanic`, so that ability lookup during gameplay can be done by card identity rather than by re-parsing effect text each time.

#### Acceptance Criteria

1. WHEN the system is compiled THEN `ability_mechanic_dictionary.h` SHALL also declare a function `pair_ability_mechanic()` returning a `const unordered_map<string, const AbilityMechanic*>&` keyed by `CardId::to_string()`
2. WHEN `pair_ability_mechanic()` is called THEN it SHALL return a static singleton map built by iterating over all known Pokémon cards with abilities and looking up their effect text in `ability_mechanic_dictionary()`
3. WHEN a Pokémon `CardId` is looked up in the map THEN the map SHALL return a non-owning pointer to the prototype `AbilityMechanic` if the card has a known ability, or `nullptr` if not
4. WHEN a Pokémon has an ability whose effect text is not in `ability_mechanic_dictionary()` THEN the lookup SHALL return `nullptr` (graceful degradation — treated as `UnknownAbilityMechanic` by callers)
5. WHEN the `Card` struct carries an `ability` field (name + effect text) THEN `pair_ability_mechanic()` SHALL use that field for the lookup

### Requirement 7: Card Struct — Ability Field

**User Story:** As a simulator developer, I want the `Card` struct to carry an optional `ability` field (name + effect text) for Pokémon cards, so that ability information is available at runtime without re-querying the database.

#### Acceptance Criteria

1. WHEN the `Card` struct is updated THEN it SHALL include an `optional<Ability>` field where `Ability` is a plain struct with `name: string` and `effect: string`
2. WHEN a Pokémon card is loaded from `database.json` and has an ability THEN the `ability` field SHALL be populated with the ability's name and effect text
3. WHEN a Pokémon card has no ability THEN the `ability` field SHALL be `std::nullopt`
4. WHEN the `Card` copy constructor and assignment operator are used THEN the `ability` field SHALL be correctly copied

### Requirement 8: Activate Ability Resolution (`UseAbility` action)

**User Story:** As a simulator developer, I want a new `UseAbility` action type and a corresponding `apply_ability_action()` function that dispatches to the correct activate-timing `AbilityMechanic`, so that player-triggered Heal abilities can be applied to the game state.

#### Acceptance Criteria

1. WHEN the `ActionType` enum is extended THEN it SHALL include a `UseAbility` variant; the `Action` struct SHALL carry a `slot_index` field identifying which Pokémon slot is using the ability
2. WHEN `apply_ability_action(gs, action, rng)` is called with a `UseAbility` action THEN it SHALL:
   - Look up the Pokémon in `slot_index`
   - Retrieve the `AbilityMechanic` via `pair_ability_mechanic()` (or treat as `UnknownAbilityMechanic` if `nullptr`)
   - Assert that the mechanic's `timing()` is `Activate`
   - Dispatch to the mechanic's `apply_activate(gs, player, slot_index, rng)` method
3. WHEN an activate ability is used THEN the `ability_used_this_turn` flag on the `InPlayPokemon` SHALL be set to `true`
4. WHEN `ability_used_this_turn` is `true` THEN the same ability SHALL NOT be usable again in the same turn
5. WHEN the turn ends (Cleanup phase) THEN `ability_used_this_turn` SHALL be reset to `false` for all in-play Pokémon
6. IF the resolved `AbilityMechanic` has `timing() == Passive` THEN `apply_ability_action()` SHALL assert/abort with a clear error message

### Requirement 9: Passive Ability Resolution — Damage Phase (`ReduceDamageFromAttacks`)

**User Story:** As a simulator developer, I want `apply_attack_damage` to consult passive `DamagePhase` abilities on the defending Pokémon, so that `ReduceDamageFromAttacks` correctly reduces incoming damage.

#### Acceptance Criteria

1. WHEN `apply_attack_damage` is called THEN it SHALL look up the defending Pokémon's `AbilityMechanic` via `pair_ability_mechanic()`
2. WHEN the defending Pokémon has a `ReduceDamageFromAttacks { amount }` ability THEN the computed damage SHALL be reduced by `amount` before being applied (minimum 0)
3. WHEN the defending Pokémon has `nullptr` or `UnknownAbilityMechanic` THEN no damage modification SHALL occur
4. WHEN weakness is also applicable THEN the reduction SHALL be applied **after** weakness (order: base damage → weakness → reduction)
5. WHEN the reduced damage is 0 or less THEN no damage counters SHALL be added to the defender

### Requirement 10: Unit Tests

**User Story:** As a simulator developer, I want unit tests covering the first-pass ability effect system, so that correctness of the rename, dictionary lookups, activate Heal abilities, and passive ReduceDamage can be verified.

#### Acceptance Criteria

1. WHEN the test suite is built THEN a new test binary `ptcgp_test_ability_effects` SHALL be compiled from `tests/test_ability_effects.cpp`
2. WHEN tests run THEN they SHALL cover:
   - **Rename regression**: `AttackMechanic` equality and clone still work correctly after rename (Req 1)
   - **`attack_mechanic_dictionary` lookup**: known effect text returns correct `AttackMechanic` prototype (Req 1)
   - **`pair_attack_mechanic` lookup**: known `CardId` + attack index returns correct pointer (Req 2)
   - **`AbilityMechanic` equality and clone**: `HealAllYourPokemon`, `HealOneYourPokemon`, `HealActiveYourPokemon`, `ReduceDamageFromAttacks`, `UnknownAbilityMechanic` (Req 3–4)
   - **`ability_mechanic_dictionary` lookup**: Heal and ReduceDamage effect texts return correct prototypes; unknown text returns absent (Req 5)
   - **`pair_ability_mechanic` lookup**: known Pokémon `CardId` returns correct pointer; unknown returns `nullptr` (Req 6)
   - **Activate Heal**: `apply_ability_action` with `HealAllYourPokemon` heals all Pokémon; `HealOneYourPokemon` heals the chosen Pokémon; `HealActiveYourPokemon` heals only the active (Req 8)
   - **Once-per-turn enforcement**: `ability_used_this_turn` is set after use; second use in same turn is blocked (Req 8)
   - **Passive ReduceDamage**: `apply_attack_damage` reduces damage by `amount` when defender has `ReduceDamageFromAttacks`; damage is clamped to 0; reduction applies after weakness (Req 9)
3. WHEN all tests pass THEN the exit code SHALL be 0
4. WHEN any test fails THEN the failure message SHALL include the file name, line number, and the failing expression
