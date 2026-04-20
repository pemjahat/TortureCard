#include "ptcgp_sim/ability_mechanic.h"
#include "ptcgp_sim/game_state.h"

#include <algorithm>
#include <string>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Internal: minimal JSON value extractor (flat object only)
// ---------------------------------------------------------------------------
static std::string ability_extract_json_value(const std::string& obj, const std::string& key)
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
// Internal: heal a single InPlayPokemon by `amount` (clamp to 0)
// ---------------------------------------------------------------------------
static void heal_pokemon(InPlayPokemon& ip, int amount)
{
    ip.damage_counters -= amount;
    if (ip.damage_counters < 0) ip.damage_counters = 0;
}

// ---------------------------------------------------------------------------
// HealAllYourPokemon::apply_activate
// ---------------------------------------------------------------------------
void HealAllYourPokemon::apply_activate(GameState& gs, int player,
                                        int /*slot_idx*/, std::mt19937& /*rng*/) const
{
    for (auto& slot : gs.players[player].pokemon_slots)
    {
        if (slot.has_value())
            heal_pokemon(*slot, amount);
    }
}

// ---------------------------------------------------------------------------
// HealOneYourPokemon::apply_activate
// Heals the Pokemon in slot_idx (the ability user's slot).
// ---------------------------------------------------------------------------
void HealOneYourPokemon::apply_activate(GameState& gs, int player,
                                        int slot_idx, std::mt19937& /*rng*/) const
{
    auto& slot = gs.players[player].pokemon_slots[slot_idx];
    if (slot.has_value())
        heal_pokemon(*slot, amount);
}

// ---------------------------------------------------------------------------
// HealActiveYourPokemon::apply_activate
// ---------------------------------------------------------------------------
void HealActiveYourPokemon::apply_activate(GameState& gs, int player,
                                           int /*slot_idx*/, std::mt19937& /*rng*/) const
{
    auto& active = gs.players[player].pokemon_slots[0];
    if (active.has_value())
        heal_pokemon(*active, amount);
}

// ---------------------------------------------------------------------------
// from_params_json implementations
// ---------------------------------------------------------------------------
void HealAllYourPokemon::from_params_json(const std::string& json)
{
    amount = std::stoi(ability_extract_json_value(json, "amount"));
}

void HealOneYourPokemon::from_params_json(const std::string& json)
{
    amount = std::stoi(ability_extract_json_value(json, "amount"));
}

void HealActiveYourPokemon::from_params_json(const std::string& json)
{
    amount = std::stoi(ability_extract_json_value(json, "amount"));
}

void ReduceDamageFromAttacks::from_params_json(const std::string& json)
{
    amount = std::stoi(ability_extract_json_value(json, "amount"));
}

} // namespace ptcgp_sim
