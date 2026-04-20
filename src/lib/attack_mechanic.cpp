#include "ptcgp_sim/attack_mechanic.h"
#include "ptcgp_sim/game_state.h"

#include <string>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Internal: minimal JSON value extractor (flat object only)
// ---------------------------------------------------------------------------
static std::string mech_extract_json_value(const std::string& obj, const std::string& key)
{
    std::string search = "\"" + key + "\":";
    auto pos = obj.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < obj.size() && obj[pos] == ' ') ++pos;
    if (pos >= obj.size()) return "";
    std::string val;
    bool in_string = (obj[pos] == '"');
    if (in_string) ++pos;
    while (pos < obj.size())
    {
        char c = obj[pos];
        if (in_string) { if (c == '"') break; val += c; }
        else           { if (c == ',' || c == '}') break; val += c; }
        ++pos;
    }
    return val;
}

// ---------------------------------------------------------------------------
// SelfHeal::apply_post_damage
// ---------------------------------------------------------------------------
void SelfHeal::apply_post_damage(InPlayPokemon& attacker) const
{
    int healed = attacker.damage_counters - amount;
    attacker.damage_counters = healed < 0 ? 0 : healed;
}

// ---------------------------------------------------------------------------
// from_params_json implementations
// ---------------------------------------------------------------------------
void FlipNCoinDamage::from_params_json(const std::string& json)
{
    coins        = std::stoi(mech_extract_json_value(json, "coins"));
    heads_damage = std::stoi(mech_extract_json_value(json, "heads_damage"));
    tails_damage = std::stoi(mech_extract_json_value(json, "tails_damage"));
}

void FlipNCoinExtraDamage::from_params_json(const std::string& json)
{
    coins                = std::stoi(mech_extract_json_value(json, "coins"));
    extra_damage         = std::stoi(mech_extract_json_value(json, "extra_damage"));
    include_fixed_damage = (mech_extract_json_value(json, "include_fixed_damage") == "true");
}

void SelfHeal::from_params_json(const std::string& json)
{
    amount = std::stoi(mech_extract_json_value(json, "amount"));
}

void FlipUntilTailsDamage::from_params_json(const std::string& json)
{
    damage_per_heads = std::stoi(mech_extract_json_value(json, "damage_per_heads"));
}

} // namespace ptcgp_sim
