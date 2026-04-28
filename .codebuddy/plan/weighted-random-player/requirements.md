# Requirements Document

## Introduction

This feature adds a `WeightedRandomPlayer` to Torture Card, modeled after `deckgym-core`'s weighted random player. The player shall choose from the currently legal Torture Card player actions using weighted random sampling, where action weights mirror the intent and numeric values from `deckgym-core`'s `WeightedRandomPlayer`.

The implementation should only choose among actions already provided in the legal move list. It should not generate new actions by itself. Card drawing remains handled by the game system rather than by a player action. Internally resolved operations, such as damage application from effects or attacks, are not player action types and should not be evaluated by `WeightedRandomPlayer`.

The CLI or simulator entry point should expose a deckgym-core-style player mode option, allowing users to select `AttachAttack` with `-player=a` or `WeightedRandom` with `-player=w`.

Unsupported `deckgym-core` action variants may be kept as reference notes or commented code near the mapping, but the runtime system does not need to identify each unsupported `deckgym-core` action until Torture Card exposes a corresponding player action. If an unsupported or unmapped Torture Card action type is ever evaluated by `WeightedRandomPlayer`, the implementation should assert or fail directly instead of assigning a silent fallback weight.

## deckgym-core Reference Weights

The source `deckgym-core` weight table assigns the following weights to `SimpleAction` variants. Rows without a Torture Card player-action mapping are reference-only notes and should not become runtime requirements unless a corresponding Torture Card `ActionType` is introduced.

| deckgym-core action | Weight | Torture Card mapping / implementation note |
| --- | ---: | --- |
| `DrawCard` | 1 | Not mapped to a Torture Card player action; draw remains handled by the game system, not by `WeightedRandomPlayer`. |
| `Play` | 5 | Maps to trainer play actions such as `PlaySupporter`, `PlayItem`, and `PlayStadium`. |
| `Place` | 5 | Maps to `PlayPokemon`. |
| `Attach` | 10 | Maps to `AttachEnergy` for turn energy attachment. |
| `MoveEnergy` | 10 | Reference-only unless Torture Card adds a corresponding player action. |
| `AttachTool` | 10 | Maps to `PlayTool`. |
| `Evolve` | 10 | Maps to `Evolve`. |
| `UseAbility` | 10 | Maps to `UseAbility`. |
| `Attack` | 10 | Maps to `Attack`. |
| `UseCopiedAttack` | 10 | Reference-only unless Torture Card adds a corresponding player action. |
| `ApplyDamage` | 10 | Not a player action type in Torture Card; damage is resolved internally by effects and attacks. |
| `ScheduleDelayedSpotDamage` | 10 | Reference-only unless Torture Card adds a corresponding player action. |
| `Retreat` | 2 | Maps to `Retreat`. |
| `EndTurn` | 1 | Maps to `Pass`. |
| `Heal` | 5 | Reference-only unless Torture Card adds a corresponding standalone player action; related effects may resolve internally. |
| `HealAndDiscardEnergy` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `MoveAllDamage` | 10 | Reference-only unless Torture Card adds a corresponding player action. |
| `Activate` | 1 | Reference-only; retreat and bench promotion are separate Torture Card mechanisms. |
| `CommunicatePokemon` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `ShufflePokemonIntoDeck` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `ShuffleOwnCardsIntoDeck` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `ShuffleOpponentSupporter` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `DiscardOpponentSupporter` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `DiscardOwnCards` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `AttachFromDiscard` | 10 | Reference-only unless Torture Card adds a corresponding player action. |
| `ApplyEeveeBagDamageBoost` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `HealAllEeveeEvolutions` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `DiscardFossil` | 1 | Reference-only unless Torture Card adds a corresponding player action. |
| `ReturnPokemonToHand` | 5 | Reference-only unless Torture Card adds a corresponding player action. |
| `UseStadium` | 5 | Reference-only as a separate activated stadium action; `PlayStadium` exists for putting a Stadium into play. |
| `Noop` | 0 | No direct Torture Card equivalent; if an equivalent action type is ever evaluated, it should assert or be explicitly mapped before use. |

## Requirements

### Requirement 1

**User Story:** As a simulation user, I want a `WeightedRandomPlayer` that chooses from legal moves using deckgym-core-inspired weights, so that Torture Card can run stochastic baseline simulations that are less arbitrary than uniform random play.

#### Acceptance Criteria

1. WHEN `WeightedRandomPlayer::decide` receives a non-empty list of legal moves THEN the system SHALL select exactly one action from that legal move list.
2. WHEN multiple legal moves are available THEN the system SHALL sample actions using positive integer weights assigned by action type.
3. WHEN the same random seed and same legal move sequence are used THEN the system SHALL produce deterministic selections for reproducible tests.
4. IF the legal move list is empty THEN the system SHALL handle the condition safely according to the existing `Player` interface conventions and SHALL NOT select an action outside the provided legal moves.
5. IF an action type has no configured player-action weight THEN the system SHALL assert or fail directly when that weight is required and SHALL NOT assign a silent fallback weight.

### Requirement 2

**User Story:** As a developer, I want Torture Card action weights to be explicitly mapped from deckgym-core, so that the C++ implementation preserves the intended action preferences.

#### Acceptance Criteria

1. WHEN a legal move has type `ActionType::PlayPokemon` THEN the system SHALL assign weight `5`, matching `Place`.
2. WHEN a legal move has type `ActionType::AttachEnergy` THEN the system SHALL assign weight `10`, matching `Attach`.
3. WHEN a legal move has type `ActionType::Attack` THEN the system SHALL assign weight `10`, matching `Attack`.
4. WHEN a legal move has type `ActionType::Retreat` THEN the system SHALL assign weight `2`, matching `Retreat`.
5. WHEN a legal move has type `ActionType::Pass` THEN the system SHALL assign weight `1`, matching `EndTurn`.
6. WHEN a legal move has type `ActionType::PlaySupporter` THEN the system SHALL assign weight `5`, matching `Play`.
7. WHEN a legal move has type `ActionType::PlayItem` THEN the system SHALL assign weight `5`, matching `Play`.
8. WHEN a legal move has type `ActionType::PlayTool` THEN the system SHALL assign weight `10`, matching `AttachTool`.
9. WHEN a legal move has type `ActionType::PlayStadium` THEN the system SHALL assign weight `5`, matching `Play` / `UseStadium` preference level.
10. WHEN a legal move has type `ActionType::Evolve` THEN the system SHALL assign weight `10`, matching `Evolve`.
11. WHEN a legal move has type `ActionType::UseAbility` THEN the system SHALL assign weight `10`, matching `UseAbility`.
12. WHEN the game needs to draw cards THEN the system SHALL handle drawing outside `WeightedRandomPlayer` and SHALL NOT require `ActionType::Draw` in the player's weighted action mapping.
13. WHEN damage is applied by attacks or effects THEN the system SHALL resolve that damage internally and SHALL NOT expose `ApplyDamage` as a `WeightedRandomPlayer` action type.

### Requirement 3

**User Story:** As a developer, I want unsupported or unmapped Torture Card action types to fail loudly when evaluated, so that missing mappings are found immediately instead of receiving accidental behavior.

#### Acceptance Criteria

1. WHEN `WeightedRandomPlayer` evaluates a known player-selectable Torture Card `ActionType` THEN the system SHALL use the explicit mapped weight.
2. IF `WeightedRandomPlayer` evaluates an unsupported or unmapped Torture Card `ActionType` THEN the system SHALL assert or fail directly.
3. WHEN unsupported `deckgym-core` variants are kept for reference THEN they MAY appear in comments, documentation, or commented-out mapping code and SHALL NOT require runtime identification until Torture Card adds a corresponding player action.
4. IF a future Torture Card action type is added for a reference-only `deckgym-core` action THEN the system SHALL require an explicit weight mapping before `WeightedRandomPlayer` can evaluate it.
5. WHEN internally resolved operations such as `ApplyDamage` are discussed in code comments or documentation THEN the system SHALL describe them as effect or attack resolution behavior, not as player-selectable action types.

### Requirement 4

**User Story:** As a test maintainer, I want unit tests for the weighted action mapping and sampling behavior, so that future action additions do not accidentally change the baseline player policy.

#### Acceptance Criteria

1. WHEN tests query the weight for each existing player-selectable `ActionType` THEN the system SHALL return the expected deckgym-core-derived value and SHALL exclude system-handled draw and internally resolved damage behavior from the player policy.
2. WHEN tests provide legal moves with different positive weights THEN the system SHALL verify that weighted sampling only selects from the provided legal moves.
3. WHEN tests use a fixed RNG seed THEN the system SHALL verify deterministic `WeightedRandomPlayer` decisions for a fixed legal move list.
4. WHEN tests provide a legal move list containing only one positive-weight action THEN the system SHALL select that action.
5. WHEN a new player-selectable `ActionType` is added without updating the weight mapping tests THEN the tests SHALL fail or otherwise surface the missing mapping.
6. WHEN tests attempt to evaluate an unsupported or unmapped action type THEN the system SHALL assert or fail directly rather than assigning a fallback weight.

### Requirement 5

**User Story:** As a Torture Card developer, I want `WeightedRandomPlayer` integrated consistently with existing players, so that it can be used in simulations and future CLI/test workflows without special-case code.

#### Acceptance Criteria

1. WHEN including the public player header THEN users SHALL be able to construct a `WeightedRandomPlayer` through a clear public interface.
2. WHEN `GameLoop` calls `decide` on `WeightedRandomPlayer` during setup or action phases THEN the player SHALL return a legal action compatible with `apply_action`.
3. WHEN `WeightedRandomPlayer` is used in setup THEN it SHALL handle setup legal moves such as `PlayPokemon` and `Pass` using the same weight table.
4. WHEN `WeightedRandomPlayer` is used during normal turns THEN it SHALL handle generated legal moves including play, evolve, attach, retreat, attack, and pass actions using the same weight table, while draw and damage resolution remain handled by the game loop and card-effect systems.
5. WHEN the CLI or simulator is launched with `-player=a` THEN the system SHALL select the existing `AttachAttack` player mode.
6. WHEN the CLI or simulator is launched with `-player=w` THEN the system SHALL select the new `WeightedRandom` player mode.
7. IF the CLI or simulator receives an unsupported `-player` value THEN the system SHALL fail clearly or print usage information and SHALL NOT silently choose a different player mode.
8. WHEN no `-player` option is provided THEN the system SHALL preserve the existing default player behavior unless the implementation explicitly documents a new default.

### Requirement 6

**User Story:** As a simulation user, I want the weighted random policy to be robust and easy to inspect, so that stochastic outcomes can be debugged when needed.

#### Acceptance Criteria

1. WHEN debugging an action decision THEN developers SHALL be able to inspect the assigned weight for each legal player action type through tests or a small helper function.
2. WHEN all available legal actions would require unsupported or unmapped weights THEN the system SHALL fail loudly and SHALL NOT fall back to arbitrary or uniform selection.
3. WHEN action weights are defined THEN they SHALL be centralized in one implementation location to avoid divergent mappings.
4. WHEN reference-only `deckgym-core` actions are mentioned in documentation or comments THEN the documentation SHALL distinguish between future possible player actions and behavior already handled internally by game systems.
