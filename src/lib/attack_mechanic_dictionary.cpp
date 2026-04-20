#include "ptcgp_sim/attack_mechanic_dictionary.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// attack_mechanic_dictionary
//
// Static prototype map: effect text -> unique_ptr<AttackMechanic>.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, std::unique_ptr<AttackMechanic>>&
attack_mechanic_dictionary()
{
    static std::unordered_map<std::string, std::unique_ptr<AttackMechanic>> MAP = []()
    {
        std::unordered_map<std::string, std::unique_ptr<AttackMechanic>> m;

        // ---------------------------------------------------------------
        // SelfHeal variants
        // ---------------------------------------------------------------
        m["Heal 10 damage from this Pokémon."]  = std::make_unique<SelfHeal>(10);
        m["Heal 20 damage from this Pokémon."]  = std::make_unique<SelfHeal>(20);
        m["Heal 30 damage from this Pokémon."]  = std::make_unique<SelfHeal>(30);
        m["Heal 40 damage from this Pokémon."]  = std::make_unique<SelfHeal>(40);
        m["Heal 50 damage from this Pokémon."]  = std::make_unique<SelfHeal>(50);
        m["Heal 60 damage from this Pokémon."]  = std::make_unique<SelfHeal>(60);

        // ---------------------------------------------------------------
        // Flip 1 coin — pure damage (no fixed damage component)
        // ---------------------------------------------------------------
        m["Flip a coin. If heads, this attack does 10 damage."]  = std::make_unique<FlipNCoinDamage>(1, 10, 0);
        m["Flip a coin. If heads, this attack does 20 damage."]  = std::make_unique<FlipNCoinDamage>(1, 20, 0);
        m["Flip a coin. If heads, this attack does 30 damage."]  = std::make_unique<FlipNCoinDamage>(1, 30, 0);
        m["Flip a coin. If heads, this attack does 40 damage."]  = std::make_unique<FlipNCoinDamage>(1, 40, 0);
        m["Flip a coin. If heads, this attack does 50 damage."]  = std::make_unique<FlipNCoinDamage>(1, 50, 0);
        m["Flip a coin. If heads, this attack does 60 damage."]  = std::make_unique<FlipNCoinDamage>(1, 60, 0);
        m["Flip a coin. If heads, this attack does 70 damage."]  = std::make_unique<FlipNCoinDamage>(1, 70, 0);
        m["Flip a coin. If heads, this attack does 80 damage."]  = std::make_unique<FlipNCoinDamage>(1, 80, 0);
        m["Flip a coin. If heads, this attack does 90 damage."]  = std::make_unique<FlipNCoinDamage>(1, 90, 0);
        m["Flip a coin. If heads, this attack does 100 damage."] = std::make_unique<FlipNCoinDamage>(1, 100, 0);

        // ---------------------------------------------------------------
        // Flip 1 coin — extra damage on top of fixed damage
        // ---------------------------------------------------------------
        m["Flip a coin. If heads, this attack does 10 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 10, true);
        m["Flip a coin. If heads, this attack does 20 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 20, true);
        m["Flip a coin. If heads, this attack does 30 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 30, true);
        m["Flip a coin. If heads, this attack does 40 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 40, true);
        m["Flip a coin. If heads, this attack does 50 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 50, true);
        m["Flip a coin. If heads, this attack does 60 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 60, true);
        m["Flip a coin. If heads, this attack does 70 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 70, true);
        m["Flip a coin. If heads, this attack does 80 more damage."] = std::make_unique<FlipNCoinExtraDamage>(1, 80, true);

        // ---------------------------------------------------------------
        // Flip 2 coins — X damage for each heads (no fixed damage)
        // ---------------------------------------------------------------
        m["Flip 2 coins. This attack does 20 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 20, false);
        m["Flip 2 coins. This attack does 30 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 30, false);
        m["Flip 2 coins. This attack does 40 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 40, false);
        m["Flip 2 coins. This attack does 50 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 50, false);
        m["Flip 2 coins. This attack does 60 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 60, false);
        m["Flip 2 coins. This attack does 80 damage for each heads."]  = std::make_unique<FlipNCoinExtraDamage>(2, 80, false);
        m["Flip 2 coins. This attack does 100 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(2, 100, false);

        // Flip 2 coins — X more damage for each heads
        m["Flip 2 coins. This attack does 20 more damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(2, 20, true);
        m["Flip 2 coins. This attack does 30 more damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(2, 30, true);
        m["Flip 2 coins. This attack does 40 more damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(2, 40, true);
        m["Flip 2 coins. This attack does 50 more damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(2, 50, true);

        // ---------------------------------------------------------------
        // Flip 3 coins — X damage for each heads
        // ---------------------------------------------------------------
        m["Flip 3 coins. This attack does 10 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 10, false);
        m["Flip 3 coins. This attack does 20 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 20, false);
        m["Flip 3 coins. This attack does 30 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 30, false);
        m["Flip 3 coins. This attack does 40 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 40, false);
        m["Flip 3 coins. This attack does 50 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 50, false);
        m["Flip 3 coins. This attack does 60 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 60, false);

        // Flip 3 coins — X more damage for each heads
        m["Flip 3 coins. This attack does 50 more damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(3, 50, true);

        // ---------------------------------------------------------------
        // Flip 4 coins — X damage for each heads
        // ---------------------------------------------------------------
        m["Flip 4 coins. This attack does 20 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(4, 20, false);
        m["Flip 4 coins. This attack does 40 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(4, 40, false);
        m["Flip 4 coins. This attack does 50 damage for each heads."] = std::make_unique<FlipNCoinExtraDamage>(4, 50, false);

        // ---------------------------------------------------------------
        // Flip until tails — X damage for each heads
        // ---------------------------------------------------------------
        m["Flip a coin until you get tails. This attack does 20 damage for each heads."] = std::make_unique<FlipUntilTailsDamage>(20);
        m["Flip a coin until you get tails. This attack does 30 damage for each heads."] = std::make_unique<FlipUntilTailsDamage>(30);
        m["Flip a coin until you get tails. This attack does 40 damage for each heads."] = std::make_unique<FlipUntilTailsDamage>(40);
        m["Flip a coin until you get tails. This attack does 60 damage for each heads."] = std::make_unique<FlipUntilTailsDamage>(60);
        m["Flip a coin until you get tails. This attack does 70 damage for each heads."] = std::make_unique<FlipUntilTailsDamage>(70);

        return m;
    }();

    return MAP;
}

// ---------------------------------------------------------------------------
// pair_attack_mechanic
//
// Static map: "<CardId::to_string()>:<attack_index>" -> AttackMechanic*.
// Built lazily from attack_mechanic_dictionary() on first call.
// NOTE: pair_attack_mechanic requires the database to be loaded first.
// For now this map is intentionally empty — it is populated by
// Database::load() after card data is available.
// ---------------------------------------------------------------------------
const std::unordered_map<std::string, const AttackMechanic*>&
pair_attack_mechanic()
{
    static std::unordered_map<std::string, const AttackMechanic*> MAP;
    return MAP;
}

} // namespace ptcgp_sim
