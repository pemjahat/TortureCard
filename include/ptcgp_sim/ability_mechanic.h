#pragma once

#include <memory>
#include <random>
#include <string>

namespace ptcgp_sim
{

struct GameState; // forward declaration for apply_activate
struct InPlayPokemon; // forward declaration

// ---------------------------------------------------------------------------
// AbilityTiming — when an ability resolves
// ---------------------------------------------------------------------------
enum class AbilityTiming
{
    Activate, // Player-triggered, once per turn during Action phase
    Passive,  // Always-on or event-triggered; no player action required
};

// ---------------------------------------------------------------------------
// PassiveHook — which game event a passive ability hooks into
// ---------------------------------------------------------------------------
enum class PassiveHook
{
    DamagePhase,  // Consulted during apply_attack_damage
    EndOfTurn,    // Triggered at end of turn (Cleanup phase)
    RetreatCost,  // Consulted when computing retreat cost
    GameState,    // Always-on game-state modifier
    OnEvolve,     // Triggered when this Pokemon evolves
    OnBenchPlay,  // Triggered when this Pokemon is played to the bench
};

// ---------------------------------------------------------------------------
// AbilityMechanic — abstract base class for all ability mechanics
//
// Each concrete mechanic implements:
//   timing()          — Activate or Passive
//   passive_hook()    — which hook (only meaningful for Passive)
//   apply_activate()  — execute the ability (only for Activate timing)
//   equals()          — value equality (used by tests)
//   clone()           — deep copy
//   type_name()       — string identifier for serialisation
// ---------------------------------------------------------------------------
struct AbilityMechanic
{
    virtual ~AbilityMechanic() = default;

    virtual AbilityTiming timing() const = 0;
    virtual PassiveHook   passive_hook() const { return PassiveHook::DamagePhase; }

    // Execute an Activate-timing ability.
    // gs       — game state to mutate
    // player   — the acting player (0 or 1)
    // slot_idx — which pokemon slot is using the ability
    // rng      — random number generator
    virtual void apply_activate(GameState& /*gs*/, int /*player*/,
                                int /*slot_idx*/, std::mt19937& /*rng*/) const {}

    virtual bool equals(const AbilityMechanic& other) const = 0;
    virtual std::unique_ptr<AbilityMechanic> clone() const = 0;
    virtual std::string type_name() const = 0;

    virtual std::string params_json() const { return "{}"; }
    virtual void from_params_json(const std::string& /*json*/) {}

    bool operator==(const AbilityMechanic& other) const { return equals(other); }
    bool operator!=(const AbilityMechanic& other) const { return !equals(other); }
};

// ---------------------------------------------------------------------------
// UnknownAbilityMechanic — no-op fallback for unrecognised ability effects.
// ---------------------------------------------------------------------------
struct UnknownAbilityMechanic : AbilityMechanic
{
    AbilityTiming timing()      const override { return AbilityTiming::Passive; }
    PassiveHook   passive_hook() const override { return PassiveHook::DamagePhase; }

    bool equals(const AbilityMechanic& other) const override
    {
        return dynamic_cast<const UnknownAbilityMechanic*>(&other) != nullptr;
    }
    std::unique_ptr<AbilityMechanic> clone() const override
    {
        return std::make_unique<UnknownAbilityMechanic>(*this);
    }
    std::string type_name() const override { return "UnknownAbility"; }
};

// ---------------------------------------------------------------------------
// HealAllYourPokemon — Activate: heal all of the player's in-play Pokemon
// ---------------------------------------------------------------------------
struct HealAllYourPokemon : AbilityMechanic
{
    int amount{0};

    HealAllYourPokemon() = default;
    explicit HealAllYourPokemon(int a) : amount(a) {}

    AbilityTiming timing() const override { return AbilityTiming::Activate; }

    void apply_activate(GameState& gs, int player, int slot_idx,
                        std::mt19937& rng) const override;

    bool equals(const AbilityMechanic& other) const override
    {
        const auto* o = dynamic_cast<const HealAllYourPokemon*>(&other);
        return o && amount == o->amount;
    }
    std::unique_ptr<AbilityMechanic> clone() const override
    {
        return std::make_unique<HealAllYourPokemon>(*this);
    }
    std::string type_name() const override { return "HealAllYourPokemon"; }

    std::string params_json() const override
    {
        return "{\"amount\":" + std::to_string(amount) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// HealOneYourPokemon — Activate: heal one chosen in-play Pokemon
// ---------------------------------------------------------------------------
struct HealOneYourPokemon : AbilityMechanic
{
    int amount{0};

    HealOneYourPokemon() = default;
    explicit HealOneYourPokemon(int a) : amount(a) {}

    AbilityTiming timing() const override { return AbilityTiming::Activate; }

    // Heals the Pokemon in slot_idx (the ability user's slot).
    // In a full engine this would present a choice; for first-pass we heal
    // the ability user's own slot as a deterministic simplification.
    void apply_activate(GameState& gs, int player, int slot_idx,
                        std::mt19937& rng) const override;

    bool equals(const AbilityMechanic& other) const override
    {
        const auto* o = dynamic_cast<const HealOneYourPokemon*>(&other);
        return o && amount == o->amount;
    }
    std::unique_ptr<AbilityMechanic> clone() const override
    {
        return std::make_unique<HealOneYourPokemon>(*this);
    }
    std::string type_name() const override { return "HealOneYourPokemon"; }

    std::string params_json() const override
    {
        return "{\"amount\":" + std::to_string(amount) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// HealActiveYourPokemon — Activate: heal the player's active Pokemon
// ---------------------------------------------------------------------------
struct HealActiveYourPokemon : AbilityMechanic
{
    int amount{0};

    HealActiveYourPokemon() = default;
    explicit HealActiveYourPokemon(int a) : amount(a) {}

    AbilityTiming timing() const override { return AbilityTiming::Activate; }

    void apply_activate(GameState& gs, int player, int slot_idx,
                        std::mt19937& rng) const override;

    bool equals(const AbilityMechanic& other) const override
    {
        const auto* o = dynamic_cast<const HealActiveYourPokemon*>(&other);
        return o && amount == o->amount;
    }
    std::unique_ptr<AbilityMechanic> clone() const override
    {
        return std::make_unique<HealActiveYourPokemon>(*this);
    }
    std::string type_name() const override { return "HealActiveYourPokemon"; }

    std::string params_json() const override
    {
        return "{\"amount\":" + std::to_string(amount) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// ReduceDamageFromAttacks — Passive (DamagePhase):
//   reduce incoming attack damage to this Pokemon by `amount` (minimum 0).
// ---------------------------------------------------------------------------
struct ReduceDamageFromAttacks : AbilityMechanic
{
    int amount{0};

    ReduceDamageFromAttacks() = default;
    explicit ReduceDamageFromAttacks(int a) : amount(a) {}

    AbilityTiming timing()      const override { return AbilityTiming::Passive; }
    PassiveHook   passive_hook() const override { return PassiveHook::DamagePhase; }

    bool equals(const AbilityMechanic& other) const override
    {
        const auto* o = dynamic_cast<const ReduceDamageFromAttacks*>(&other);
        return o && amount == o->amount;
    }
    std::unique_ptr<AbilityMechanic> clone() const override
    {
        return std::make_unique<ReduceDamageFromAttacks>(*this);
    }
    std::string type_name() const override { return "ReduceDamageFromAttacks"; }

    std::string params_json() const override
    {
        return "{\"amount\":" + std::to_string(amount) + "}";
    }
    void from_params_json(const std::string& json) override;
};

} // namespace ptcgp_sim
