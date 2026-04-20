#pragma once

#include "attack_mechanic.h"
#include "ability_mechanic.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ptcgp_sim 
{

enum class CardType 
{
    Pokemon,
    Trainer,
    Energy,
};

// Sub-classification for Trainer cards
enum class TrainerType
{
    Item,       // One-shot effect, no per-turn limit
    Tool,       // Attaches to a Pokemon; one Tool per Pokemon at a time
    Supporter,  // Powerful effect; limited to one per turn
    Stadium,    // Placed in shared zone; replaces any existing Stadium
};

enum class EnergyType 
{
    Grass,
    Fire,
    Water,
    Lightning,
    Psychic,
    Fighting,
    Darkness,
    Metal,
    Dragon,
    Colorless,
    Unknown,
};

// Structured card identifier: expansion code (e.g. "A1", "B2b") + card number (e.g. 1 for "001")
struct CardId 
{
    std::string expansion; // 2-3 character set code, e.g. "A1", "B2b"
    int         number{0}; // card number stored as int, e.g. "001" -> 1

    // Reconstruct the original full id string, e.g. "A1 001"
    std::string to_string() const;

    bool operator==(const CardId& o) const 
    {
        return expansion == o.expansion && number == o.number;
    }
    bool operator!=(const CardId& o) const { return !(*this == o); }
};

// A single attack on a Pokemon card
struct Attack
{
    std::string             name;
    std::vector<EnergyType> energy_required;           // energy cost to use this attack
    int                     damage{0};                 // base fixed damage dealt
    std::optional<std::string> effect{};               // raw effect text from database.json

    // Resolved mechanic — nullptr means BasicDamage (use fixed damage only).
    std::unique_ptr<AttackMechanic> mechanic{};

    // Default constructor
    Attack() = default;

    // Copy constructor — deep-copies the mechanic via clone()
    Attack(const Attack& other)
        : name(other.name)
        , energy_required(other.energy_required)
        , damage(other.damage)
        , effect(other.effect)
        , mechanic(other.mechanic ? other.mechanic->clone() : nullptr)
    {}


    // Copy assignment
    Attack& operator=(const Attack& other)
    {
        if (this != &other)
        {
            name             = other.name;
            energy_required  = other.energy_required;
            damage           = other.damage;
            effect           = other.effect;
            mechanic         = other.mechanic ? other.mechanic->clone() : nullptr;
        }
        return *this;
    }

    // Move constructor / assignment — default is fine (unique_ptr moves cleanly)
    Attack(Attack&&) noexcept = default;
    Attack& operator=(Attack&&) noexcept = default;
};

// Ability data for a Pokemon card.
// `mechanic` is resolved by Database::resolve_mechanics() from pair_mechanic.json.
// Never nullptr after resolution — UnknownAbilityMechanic is used as fallback.
struct Ability
{
    std::string name;
    std::string effect; // raw text kept for diagnostics / dictionary rebuild only

    // Resolved mechanic — set by resolve_mechanics(), never accessed by string key at runtime.
    std::unique_ptr<AbilityMechanic> mechanic{};

    Ability() = default;

    // Copy constructor — deep-copies the mechanic via clone()
    Ability(const Ability& other)
        : name(other.name)
        , effect(other.effect)
        , mechanic(other.mechanic ? other.mechanic->clone() : nullptr)
    {}

    // Copy assignment
    Ability& operator=(const Ability& other)
    {
        if (this != &other)
        {
            name     = other.name;
            effect   = other.effect;
            mechanic = other.mechanic ? other.mechanic->clone() : nullptr;
        }
        return *this;
    }

    // Move constructor / assignment — default is fine
    Ability(Ability&&) noexcept = default;
    Ability& operator=(Ability&&) noexcept = default;
};

struct Card 
{
    CardId                    id;
    std::string               name;
    CardType                  type{CardType::Pokemon};

    // --- Pokemon fields ---
    int                       hp{0};
    EnergyType                energy_type{EnergyType::Colorless};
    std::optional<EnergyType> weakness;
    std::vector<EnergyType>   retreat_cost;
    int                       stage{0};       // 0 = Basic, 1 = Stage 1, 2 = Stage 2
    std::optional<std::string> evolves_from{}; // name of the pre-evolution (nullopt for Basic)
    std::vector<Attack>       attacks;
    std::optional<Ability>    ability{};      // Pokemon ability (nullopt if none)

    // --- Trainer fields ---
    TrainerType               trainer_type{TrainerType::Item}; // meaningful only when type == Trainer

    // --- Classification helpers ---

    // Returns true if this is a Pokemon whose name ends with " ex"
    // (e.g. "Mewtwo ex", "Charizard ex").  Always false for non-Pokemon cards.
    bool is_ex() const
    {
        if (type != CardType::Pokemon) return false;
        const std::string suffix = " ex";
        if (name.size() <= suffix.size()) return false;
        return name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // Returns true if this is a Pokemon whose name starts with "Mega "
    // (e.g. "Mega Charizard", "Mega Mewtwo").  Always false for non-Pokemon cards.
    bool is_mega() const
    {
        if (type != CardType::Pokemon) return false;
        const std::string prefix = "Mega ";
        if (name.size() <= prefix.size()) return false;
        return name.compare(0, prefix.size(), prefix) == 0;
    }

    // Returns the number of prize points awarded to the opponent when this
    // Pokemon is knocked out: 3 for Mega, 2 for ex, 1 for regular, 0 for Trainers.
    int knockout_points() const
    {
        if (type != CardType::Pokemon) return 0;
        if (is_mega()) return 3;
        if (is_ex())   return 2;
        return 1;
    }
};

} // namespace ptcgp_sim
