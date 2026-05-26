#include "ptcgp_sim/toolitem_effects.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/action.h"

#include <cassert>
#include <iostream>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Card identity predicates
// ---------------------------------------------------------------------------

// A1 088 = Rocky Helmet (Genetic Apex)
static bool is_rocky_helmet(const CardId& id)
{
    return id.expansion == "A1" && id.number == 88;
}

// A2 147 = Giant Cape (Space-Time Smackdown)
static bool is_giant_cape(const CardId& id)
{
    return id.expansion == "A2" && id.number == 147;
}

// A1 196 = Potion (Genetic Apex)
// PA  013 = Potion (Promo-A)
static bool is_potion(const CardId& id)
{
    if (id.expansion == "A1" && id.number == 196) return true;
    if (id.expansion == "PA" && id.number == 13)  return true;
    return false;
}

// ---------------------------------------------------------------------------
// effective_hp
// ---------------------------------------------------------------------------

int effective_hp(const InPlayPokemon& ip)
{
    int hp = ip.card.hp;
    if (ip.attached_tool.has_value() && is_giant_cape(ip.attached_tool->id))
        hp += 20;
    return hp;
}

// ---------------------------------------------------------------------------
// is_potion_item  (public helper used by move_generation.cpp)
// ---------------------------------------------------------------------------

bool is_potion_item(const CardId& id)
{
    return is_potion(id);
}

// ---------------------------------------------------------------------------
// Item effect implementations
// ---------------------------------------------------------------------------

// Potion: heal 20 damage from the target Pokemon (clamped to 0).
static void apply_potion_effect(GameState& gs, int player, int target_slot)
{
    assert(target_slot >= 0 && target_slot <= 3 &&
           "apply_potion_effect: target_slot must be 0-3");

    auto& slot = gs.players[player].pokemon_slots[target_slot];
    assert(slot.has_value() && "apply_potion_effect: target slot is empty");

    InPlayPokemon& ip = *slot;
    ip.damage_counters -= 20;
    if (ip.damage_counters < 0)
        ip.damage_counters = 0;
}

// ---------------------------------------------------------------------------
// apply_item_effect
// ---------------------------------------------------------------------------

void apply_item_effect(GameState& gs, int player, const CardId& card_id,
                       int target_slot, std::mt19937& /*rng*/)
{
    if (is_potion(card_id))
    {
        apply_potion_effect(gs, player, target_slot);
    }
    else
    {
        // Unimplemented item — card is already discarded; warn and continue.
        std::cerr << "[WARN] apply_item_effect: unimplemented item effect for "
                  << card_id.to_string() << "\n";
    }
}

// ---------------------------------------------------------------------------
// apply_tool_passive
// ---------------------------------------------------------------------------

void apply_tool_passive(GameState& gs, int attacker_player, int damage_dealt)
{
    // No damage — no passive triggers.
    if (damage_dealt <= 0)
        return;

    const int defender_player = (attacker_player + 1) % 2;

    // Only the directly-attacked slot (active, slot 0) triggers counterattack.
    auto& defender_slot = gs.players[defender_player].pokemon_slots[0];
    if (!defender_slot.has_value())
        return;

    const InPlayPokemon& defender = *defender_slot;

    // Rocky Helmet: deal 20 damage back to the attacker.
    if (defender.attached_tool.has_value() && is_rocky_helmet(defender.attached_tool->id))
    {
        auto& attacker_slot = gs.players[attacker_player].pokemon_slots[0];
        if (attacker_slot.has_value())
        {
            InPlayPokemon& attacker = *attacker_slot;
            int new_counters = attacker.damage_counters + 20;
            attacker.damage_counters = std::min(new_counters, attacker.card.hp);
        }
    }
}

} // namespace ptcgp_sim
