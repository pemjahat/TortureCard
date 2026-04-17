#pragma once

#include <memory>
#include <random>
#include <string>

namespace ptcgp_sim
{

struct InPlayPokemon; // forward declaration for apply_post_damage

// ---------------------------------------------------------------------------
// Mechanic — abstract base class for all attack mechanics
//
// Each concrete mechanic implements:
//   compute_damage  — returns the raw damage to deal (before weakness).
//                     fixed_damage is the attack's base damage field.
//   apply_post_damage — called after damage + weakness are applied to the
//                       defender; used for side effects on the attacker
//                       (e.g. SelfHeal).  Default: no-op.
//   equals          — value equality (used by tests).
//   clone           — deep copy (needed because Attack stores unique_ptr).
//   type_name       — string identifier for serialisation.
// ---------------------------------------------------------------------------
struct Mechanic
{
    virtual ~Mechanic() = default;

    virtual int  compute_damage(int fixed_damage, std::mt19937& rng) const = 0;
    virtual void apply_post_damage(InPlayPokemon& /*attacker*/) const {}
    virtual bool equals(const Mechanic& other) const = 0;
    virtual std::unique_ptr<Mechanic> clone() const = 0;
    virtual std::string type_name() const = 0;

    // Serialise this mechanic's parameters to a flat JSON object string.
    // e.g. {"coins":2,"heads_damage":30,"tails_damage":0}
    // Default (no params): returns "{}"
    virtual std::string params_json() const { return "{}"; }

    // Deserialise parameters from a flat JSON object string produced by params_json().
    // Called on a default-constructed instance immediately after construction.
    virtual void from_params_json(const std::string& /*json*/) {}

    bool operator==(const Mechanic& other) const { return equals(other); }
    bool operator!=(const Mechanic& other) const { return !equals(other); }
};

// ---------------------------------------------------------------------------
// BasicDamage — deals only the fixed damage field, no extra effect.
// ---------------------------------------------------------------------------
struct BasicDamage : Mechanic
{
    int compute_damage(int fixed_damage, std::mt19937&) const override
    {
        return fixed_damage;
    }
    bool equals(const Mechanic& other) const override
    {
        return dynamic_cast<const BasicDamage*>(&other) != nullptr;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<BasicDamage>(*this);
    }
    std::string type_name() const override { return "BasicDamage"; }
    // No params — inherits default params_json() / from_params_json()
};

// ---------------------------------------------------------------------------
// FlipNCoinDamage — flip N coins; damage = heads_damage*heads + tails_damage*tails.
// Replaces fixed damage entirely.
// ---------------------------------------------------------------------------
struct FlipNCoinDamage : Mechanic
{
    int coins{1};
    int heads_damage{0};
    int tails_damage{0};

    FlipNCoinDamage() = default;
    FlipNCoinDamage(int c, int hd, int td) : coins(c), heads_damage(hd), tails_damage(td) {}

    int compute_damage(int /*fixed_damage*/, std::mt19937& rng) const override
    {
        std::bernoulli_distribution coin(0.5);
        int heads = 0, tails = 0;
        for (int i = 0; i < coins; ++i)
        {
            if (coin(rng)) ++heads;
            else           ++tails;
        }
        return heads_damage * heads + tails_damage * tails;
    }
    bool equals(const Mechanic& other) const override
    {
        const auto* o = dynamic_cast<const FlipNCoinDamage*>(&other);
        return o && coins == o->coins && heads_damage == o->heads_damage
                 && tails_damage == o->tails_damage;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<FlipNCoinDamage>(*this);
    }
    std::string type_name() const override { return "FlipNCoinDamage"; }

    std::string params_json() const override
    {
        return "{\"coins\":" + std::to_string(coins)
             + ",\"heads_damage\":" + std::to_string(heads_damage)
             + ",\"tails_damage\":" + std::to_string(tails_damage) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// FlipNCoinExtraDamage — flip N coins;
//   damage = (include_fixed_damage ? fixed_damage : 0) + extra_damage * heads.
// ---------------------------------------------------------------------------
struct FlipNCoinExtraDamage : Mechanic
{
    int  coins{1};
    int  extra_damage{0};
    bool include_fixed_damage{false};

    FlipNCoinExtraDamage() = default;
    FlipNCoinExtraDamage(int c, int ed, bool ifd)
        : coins(c), extra_damage(ed), include_fixed_damage(ifd) {}

    int compute_damage(int fixed_damage, std::mt19937& rng) const override
    {
        std::bernoulli_distribution coin(0.5);
        int heads = 0;
        for (int i = 0; i < coins; ++i)
            if (coin(rng)) ++heads;
        int base = include_fixed_damage ? fixed_damage : 0;
        return base + extra_damage * heads;
    }
    bool equals(const Mechanic& other) const override
    {
        const auto* o = dynamic_cast<const FlipNCoinExtraDamage*>(&other);
        return o && coins == o->coins && extra_damage == o->extra_damage
                 && include_fixed_damage == o->include_fixed_damage;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<FlipNCoinExtraDamage>(*this);
    }
    std::string type_name() const override { return "FlipNCoinExtraDamage"; }

    std::string params_json() const override
    {
        return "{\"coins\":" + std::to_string(coins)
             + ",\"extra_damage\":" + std::to_string(extra_damage)
             + ",\"include_fixed_damage\":" + (include_fixed_damage ? "true" : "false") + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// SelfHeal — deals fixed damage, then heals `amount` from the attacker.
// ---------------------------------------------------------------------------
struct SelfHeal : Mechanic
{
    int amount{0};

    SelfHeal() = default;
    explicit SelfHeal(int a) : amount(a) {}

    int compute_damage(int fixed_damage, std::mt19937&) const override
    {
        return fixed_damage;
    }
    void apply_post_damage(InPlayPokemon& attacker) const override;

    bool equals(const Mechanic& other) const override
    {
        const auto* o = dynamic_cast<const SelfHeal*>(&other);
        return o && amount == o->amount;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<SelfHeal>(*this);
    }
    std::string type_name() const override { return "SelfHeal"; }

    std::string params_json() const override
    {
        return "{\"amount\":" + std::to_string(amount) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// FlipUntilTailsDamage — flip coins until tails; damage = damage_per_heads * heads.
// ---------------------------------------------------------------------------
struct FlipUntilTailsDamage : Mechanic
{
    int damage_per_heads{0};

    FlipUntilTailsDamage() = default;
    explicit FlipUntilTailsDamage(int dph) : damage_per_heads(dph) {}

    int compute_damage(int /*fixed_damage*/, std::mt19937& rng) const override
    {
        std::bernoulli_distribution coin(0.5);
        int heads = 0;
        while (coin(rng)) ++heads;
        return damage_per_heads * heads;
    }
    bool equals(const Mechanic& other) const override
    {
        const auto* o = dynamic_cast<const FlipUntilTailsDamage*>(&other);
        return o && damage_per_heads == o->damage_per_heads;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<FlipUntilTailsDamage>(*this);
    }
    std::string type_name() const override { return "FlipUntilTailsDamage"; }

    std::string params_json() const override
    {
        return "{\"damage_per_heads\":" + std::to_string(damage_per_heads) + "}";
    }
    void from_params_json(const std::string& json) override;
};

// ---------------------------------------------------------------------------
// UnknownMechanic — placeholder for unrecognised effect text.
// Falls back to fixed damage at runtime.
// ---------------------------------------------------------------------------
struct UnknownMechanic : Mechanic
{
    int compute_damage(int fixed_damage, std::mt19937&) const override
    {
        return fixed_damage;
    }
    bool equals(const Mechanic& other) const override
    {
        return dynamic_cast<const UnknownMechanic*>(&other) != nullptr;
    }
    std::unique_ptr<Mechanic> clone() const override
    {
        return std::make_unique<UnknownMechanic>(*this);
    }
    std::string type_name() const override { return "Unknown"; }
    // No params — inherits default params_json() / from_params_json()
};

} // namespace ptcgp_sim
