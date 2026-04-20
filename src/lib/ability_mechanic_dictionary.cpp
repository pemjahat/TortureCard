#include "ptcgp_sim/ability_mechanic_dictionary.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// ability_mechanic_dictionary
//
// Static prototype map: ability effect text -> unique_ptr<AbilityMechanic>.
//
// First-pass scope:
//   - Heal family (Activate): HealAllYourPokemon, HealOneYourPokemon,
//                              HealActiveYourPokemon
//   - ReduceDamage family (Passive/DamagePhase): ReduceDamageFromAttacks
//
// All other ability effect texts are absent from this map; callers treat
// absence as UnknownAbilityMechanic.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, std::unique_ptr<AbilityMechanic>>&
ability_mechanic_dictionary()
{
    static std::unordered_map<std::string, std::unique_ptr<AbilityMechanic>> MAP = []()
    {
        std::unordered_map<std::string, std::unique_ptr<AbilityMechanic>> m;

        // ---------------------------------------------------------------
        // HealAllYourPokemon variants
        // ---------------------------------------------------------------
        m["Once during your turn, you may heal 10 damage from each of your Pokémon."]
            = std::make_unique<HealAllYourPokemon>(10);
        m["Once during your turn, you may heal 20 damage from each of your Pokémon."]
            = std::make_unique<HealAllYourPokemon>(20);

        // ---------------------------------------------------------------
        // HealOneYourPokemon variants
        // ---------------------------------------------------------------
        m["Once during your turn, if this Pokémon is in the Active Spot, you may heal 30 damage from 1 of your Pokémon."]
            = std::make_unique<HealOneYourPokemon>(30);

        // ---------------------------------------------------------------
        // HealActiveYourPokemon variants
        // ---------------------------------------------------------------
        m["Once during your turn, you may heal 20 damage from your Active Pokémon."]
            = std::make_unique<HealActiveYourPokemon>(20);

        // ---------------------------------------------------------------
        // ReduceDamageFromAttacks variants
        // ---------------------------------------------------------------
        m["This Pokémon takes -20 damage from attacks."]
            = std::make_unique<ReduceDamageFromAttacks>(20);
        m["This Pokémon takes -30 damage from attacks."]
            = std::make_unique<ReduceDamageFromAttacks>(30);

        return m;
    }();

    return MAP;
}

// ---------------------------------------------------------------------------
// pair_ability_mechanic
//
// Static map: CardId::to_string() -> AbilityMechanic*.
// Built lazily from ability_mechanic_dictionary() on first call.
// NOTE: pair_ability_mechanic requires the database to be loaded first.
// For now this map is intentionally empty — it is populated by
// Database::load() after card data is available.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, const AbilityMechanic*>&
pair_ability_mechanic()
{
    static std::unordered_map<std::string, const AbilityMechanic*> MAP;
    return MAP;
}

} // namespace ptcgp_sim
