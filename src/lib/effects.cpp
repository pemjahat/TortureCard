#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/card.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/action.h"
#include "ptcgp_sim/ability_mechanic.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Internal: known "search basic Pokemon" item card IDs
// ---------------------------------------------------------------------------

// Returns true when the given CardId corresponds to a Poke Ball or any other
// item whose effect is "search your deck for a Basic Pokemon, put it in hand,
// shuffle deck".  Extend this list as more sets are added.
static bool is_search_basic_item(const CardId& id)
{
    // PA 005 = Poke Ball (Promo-A)
    // A2b 111 = Poke Ball (Space-Time Smackdown promo reprint)
    if (id.expansion == "PA"  && id.number == 5)   return true;
    if (id.expansion == "A2b" && id.number == 111) return true;
    return false;
}

// ---------------------------------------------------------------------------
// search_basic_to_hand
// ---------------------------------------------------------------------------

bool search_basic_to_hand(GameState& gs, int player, std::mt19937& rng)
{
    auto& deck = gs.players[player].deck;
    auto& hand = gs.players[player].hand;

    // Find the first Basic Pokemon in the deck
    auto it = std::find_if(deck.cards.begin(), deck.cards.end(),
        [](const Card& c)
        {
            return c.type == CardType::Pokemon && c.stage == 0;
        });

    bool found = (it != deck.cards.end());
    if (found)
    {
        hand.push_back(*it);
        deck.cards.erase(it);
    }

    // Always shuffle after a search (whether or not a card was found)
    deck.shuffle(rng);
    return found;
}

// ---------------------------------------------------------------------------
// apply_attack_damage
// ---------------------------------------------------------------------------

int apply_attack_damage(GameState& gs, int attacker_player, int attack_index, std::mt19937& rng)
{
    const int defender_player = (attacker_player + 1) % 2;

    auto& attacker_slot = gs.players[attacker_player].pokemon_slots[0];
    auto& defender_slot = gs.players[defender_player].pokemon_slots[0];

    assert(attacker_slot.has_value() && "apply_attack_damage: attacker has no active Pokemon");
    assert(defender_slot.has_value() && "apply_attack_damage: defender has no active Pokemon");

    InPlayPokemon& attacker = *attacker_slot;
    InPlayPokemon& defender = *defender_slot;

    assert(attack_index >= 0 &&
           attack_index < static_cast<int>(attacker.card.attacks.size()) &&
           "apply_attack_damage: attack_index out of range");

    const Attack& atk = attacker.card.attacks[attack_index];

    // -----------------------------------------------------------------------
    // Compute raw damage via virtual dispatch
    // -----------------------------------------------------------------------
    int damage = 0;

    if (!atk.mechanic)
    {
        // No mechanic resolved — use fixed damage (BasicDamage behaviour)
        damage = atk.damage;
    }
    else
    {
        damage = atk.mechanic->compute_damage(atk.damage, rng);
    }

    // -----------------------------------------------------------------------
    // Apply weakness: +20 if defender's weakness matches attacker's energy type
    // -----------------------------------------------------------------------
    if (damage > 0 &&
        defender.card.weakness.has_value() &&
        *defender.card.weakness == attacker.card.energy_type)
    {
        damage += 20;
    }

    // -----------------------------------------------------------------------
    // Apply passive DamagePhase ability: ReduceDamageFromAttacks
    // Order: base damage -> weakness -> reduction (minimum 0)
    // -----------------------------------------------------------------------
    if (damage > 0 && defender.card.ability.has_value())
    {
        const AbilityMechanic* mech = defender.card.ability->mechanic.get();
        if (mech &&
            mech->timing()       == AbilityTiming::Passive &&
            mech->passive_hook() == PassiveHook::DamagePhase)
        {
            const auto* reduce = dynamic_cast<const ReduceDamageFromAttacks*>(mech);
            if (reduce)
            {
                damage -= reduce->amount;
                if (damage < 0) damage = 0;
            }
        }
    }

    // Apply damage (cap so damage_counters never exceed card.hp)
    if (damage > 0)
    {
        int new_counters = defender.damage_counters + damage;
        defender.damage_counters = std::min(new_counters, defender.card.hp);
    }

    // -----------------------------------------------------------------------
    // Post-damage effects (e.g. SelfHeal) via virtual dispatch
    // -----------------------------------------------------------------------
    if (atk.mechanic)
    {
        atk.mechanic->apply_post_damage(attacker);
    }

    return damage;
}

// ---------------------------------------------------------------------------
// resolve_knockouts
// ---------------------------------------------------------------------------

void resolve_knockouts(GameState& gs, int attacking_player)
{
    // Collect all knocked-out Pokemon (both sides) before modifying slots
    struct KOEntry { int player; int slot; };
    std::vector<KOEntry> knockouts;

    for (int p = 0; p < 2; ++p)
    {
        for (int s = 0; s < 4; ++s)
        {
            if (gs.players[p].pokemon_slots[s].has_value() &&
                gs.players[p].pokemon_slots[s]->is_knocked_out())
            {
                knockouts.push_back({p, s});
            }
        }
    }

    if (knockouts.empty()) return;

    // Process each knockout
    for (const auto& ko : knockouts)
    {
        const int ko_player   = ko.player;
        const int ko_slot     = ko.slot;
        const int point_winner = (ko_player + 1) % 2;

        auto& slot = gs.players[ko_player].pokemon_slots[ko_slot];
        assert(slot.has_value());

        InPlayPokemon& ip = *slot;

        // Move all lower-stage cards (evolution chain) to discard pile first
        for (const Card& behind : ip.cards_behind)
            gs.players[ko_player].discard_pile.push_back(behind);

        // Move Pokemon card to discard pile
        gs.players[ko_player].discard_pile.push_back(ip.card);

        // Move attached tool (if any) to discard pile
        if (ip.attached_tool.has_value())
            gs.players[ko_player].discard_pile.push_back(*ip.attached_tool);

        // Move attached energies to the energy discard bin
        for (EnergyType e : ip.attached_energy)
            gs.players[ko_player].energy_discard.push_back(e);

        // Award prize points: 1 for regular, 2 for ex, 3 for Mega
        const int pts = ip.card.knockout_points();

        // Clear the slot
        slot = std::nullopt;

        // Award prize points to the opponent
        gs.players[point_winner].points += pts;
    }

    // Check win conditions after all KOs are processed

    // Both players simultaneously reach >= 3 points -> tie
    if (gs.players[0].points >= 3 && gs.players[1].points >= 3)
    {
        gs.game_over = true;
        gs.winner    = -1; // tie
        return;
    }

    for (int p = 0; p < 2; ++p)
    {
        if (gs.players[p].points >= 3)
        {
            gs.game_over = true;
            gs.winner    = p;
            return;
        }
    }

    // Check if any active slot was knocked out and the owner has no bench left
    for (const auto& ko : knockouts)
    {
        if (ko.slot != 0) continue; // only care about active KOs

        const int ko_player  = ko.player;
        const int opponent   = (ko_player + 1) % 2;

        // Count remaining bench Pokemon for the KO'd player
        bool has_bench = false;
        for (int s = 1; s <= 3; ++s)
        {
            if (gs.players[ko_player].pokemon_slots[s].has_value())
            {
                has_bench = true;
                break;
            }
        }

        if (!has_bench)
        {
            gs.game_over = true;
            gs.winner    = opponent;
            return;
        }
        // If bench exists, active slot is simply empty — promotion is a
        // separate player decision (out of scope for this iteration).
    }

    (void)attacking_player; // reserved for future counterattack logic
}

// ---------------------------------------------------------------------------
// apply_action
// ---------------------------------------------------------------------------

void apply_action(GameState& gs, const Action& action, std::mt19937& rng)
{
    const int player = gs.current_player;

    switch (action.type)
    {
        // ---------------------------------------------------------------
        // Attack
        // ---------------------------------------------------------------
        case ActionType::Attack:
        {
            assert(!gs.attacked_this_turn && "apply_action: already attacked this turn");
            assert(gs.players[player].pokemon_slots[0].has_value() &&
                   "apply_action: no active Pokemon to attack with");

            apply_attack_damage(gs, player, action.attack_index, rng);
            resolve_knockouts(gs, player);

            gs.attacked_this_turn = true;
            break;
        }

        // ---------------------------------------------------------------
        // PlayItem
        // ---------------------------------------------------------------
        case ActionType::PlayItem:
        {
            auto& hand = gs.players[player].hand;

            // Find and remove the item card from hand
            auto it = std::find_if(hand.begin(), hand.end(),
                [&](const Card& c){ return c.id == action.card_id; });
            assert(it != hand.end() && "apply_action: PlayItem card not found in hand");

            Card item_card = *it;
            hand.erase(it);
            gs.players[player].discard_pile.push_back(item_card);

            // Dispatch to the appropriate item effect
            if (is_search_basic_item(action.card_id))
            {
                search_basic_to_hand(gs, player, rng);
            }
            // Unknown items: card is already discarded — no further effect (no crash)
            break;
        }

        // ---------------------------------------------------------------
        // PlayTool
        // ---------------------------------------------------------------
        case ActionType::PlayTool:
        {
            auto& hand = gs.players[player].hand;
            auto it = std::find_if(hand.begin(), hand.end(),
                [&](const Card& c){ return c.id == action.card_id; });
            assert(it != hand.end() && "apply_action: PlayTool card not found in hand");

            Card tool_card = *it;
            hand.erase(it);

            auto& target_slot = gs.players[player].pokemon_slots[action.slot_index];
            assert(target_slot.has_value() && "apply_action: PlayTool target slot is empty");
            assert(!target_slot->attached_tool.has_value() &&
                   "apply_action: PlayTool target slot already has a tool");

            target_slot->attached_tool = tool_card;
            break;
        }

        // ---------------------------------------------------------------
        // PlayPokemon (Basic only — place from hand onto empty slot)
        // ---------------------------------------------------------------
        case ActionType::PlayPokemon:
        {
            auto& hand = gs.players[player].hand;
            auto it = std::find_if(hand.begin(), hand.end(),
                [&](const Card& c){ return c.id == action.card_id; });
            assert(it != hand.end() && "apply_action: PlayPokemon card not found in hand");

            Card pokemon_card = *it;
            hand.erase(it);

            auto& target_slot = gs.players[player].pokemon_slots[action.slot_index];
            assert(!target_slot.has_value() && "apply_action: PlayPokemon target slot is occupied");

            InPlayPokemon ip;
            ip.card             = pokemon_card;
            ip.played_this_turn = true;
            target_slot         = ip;
            break;
        }

        // ---------------------------------------------------------------
        // AttachEnergy
        // ---------------------------------------------------------------
        case ActionType::AttachEnergy:
        {
            assert(!gs.energy_attached_this_turn && "apply_action: energy already attached this turn");
            assert(gs.current_energy.has_value() && "apply_action: no energy available to attach");

            auto& target_slot = gs.players[player].pokemon_slots[action.target_slot];
            assert(target_slot.has_value() && "apply_action: AttachEnergy target slot is empty");

            target_slot->attached_energy.push_back(action.energy_type);
            gs.energy_attached_this_turn = true;
            gs.current_energy = std::nullopt;
            break;
        }

        // ---------------------------------------------------------------
        // Retreat
        // ---------------------------------------------------------------
        case ActionType::Retreat:
        {
            assert(!gs.retreated_this_turn && "apply_action: already retreated this turn");

            auto& active_slot = gs.players[player].pokemon_slots[0];
            auto& bench_slot  = gs.players[player].pokemon_slots[action.slot_index];

            assert(active_slot.has_value() && "apply_action: Retreat — no active Pokemon");
            assert(bench_slot.has_value()  && "apply_action: Retreat — bench slot is empty");

            // Pay retreat cost: retreat cost is always Colorless (any energy counts).
            // Remove one energy per cost entry and send it to the energy discard bin.
            InPlayPokemon& active = *active_slot;
            const int cost_count = static_cast<int>(active.card.retreat_cost.size());
            for (int i = 0; i < cost_count && !active.attached_energy.empty(); ++i)
            {
                gs.players[player].energy_discard.push_back(active.attached_energy.back());
                active.attached_energy.pop_back();
            }

            // Clear volatile status on the retreating Pokemon
            active.clear_volatile_status();

            // Swap active and bench
            std::swap(active_slot, bench_slot);
            gs.retreated_this_turn = true;
            break;
        }

        // ---------------------------------------------------------------
        // PlaySupporter
        // ---------------------------------------------------------------
        case ActionType::PlaySupporter:
        {
            assert(!gs.supporter_played_this_turn &&
                   "apply_action: supporter already played this turn");

            auto& hand = gs.players[player].hand;
            auto it = std::find_if(hand.begin(), hand.end(),
                [&](const Card& c){ return c.id == action.card_id; });
            assert(it != hand.end() && "apply_action: PlaySupporter card not found in hand");

            Card supporter_card = *it;
            hand.erase(it);
            gs.players[player].discard_pile.push_back(supporter_card);
            gs.supporter_played_this_turn = true;
            // Supporter effects are out of scope for this iteration
            break;
        }

        // ---------------------------------------------------------------
        // PlayStadium
        // ---------------------------------------------------------------
        case ActionType::PlayStadium:
        {
            auto& hand = gs.players[player].hand;
            auto it = std::find_if(hand.begin(), hand.end(),
                [&](const Card& c){ return c.id == action.card_id; });
            assert(it != hand.end() && "apply_action: PlayStadium card not found in hand");

            Card stadium_card = *it;
            hand.erase(it);

            // Discard the old stadium if one is active
            if (gs.current_stadium.has_value())
            {
                // We only store the CardId of the active stadium, not the full Card.
                // The old stadium ID is simply replaced.
            }
            gs.current_stadium = stadium_card.id;
            gs.players[player].discard_pile.push_back(stadium_card);
            break;
        }

        // ---------------------------------------------------------------
        // ---------------------------------------------------------------
        // Evolve — play an evolution card from hand onto an in-play Pokemon
        // ---------------------------------------------------------------
        case ActionType::Evolve:
        {
            apply_evolve(gs, player, action.card_id, action.slot_index);
            break;
        }

        // ---------------------------------------------------------------
        // UseAbility
        // ---------------------------------------------------------------
        case ActionType::UseAbility:
        {
            apply_ability_action(gs, action, rng);
            break;
        }

        // ---------------------------------------------------------------
        // Pass / Draw — no state mutation needed here
        // ---------------------------------------------------------------
        case ActionType::Pass:
        case ActionType::Draw:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// apply_evolve
// ---------------------------------------------------------------------------

void apply_evolve(GameState& gs, int player, const CardId& evolution_card_id, int slot_index)
{
    auto& hand = gs.players[player].hand;
    auto& target_slot = gs.players[player].pokemon_slots[slot_index];

    // Find the evolution card in hand
    auto it = std::find_if(hand.begin(), hand.end(),
        [&](const Card& c){ return c.id == evolution_card_id; });
    assert(it != hand.end() && "apply_evolve: evolution card not found in hand");
    Card evolution_card = *it;

    // Validate target slot
    assert(target_slot.has_value() && "apply_evolve: target slot is empty");
    InPlayPokemon& from = *target_slot;

    assert(!from.played_this_turn &&
           "apply_evolve: cannot evolve a Pokemon that was placed this turn");
    assert(evolution_card.stage > 0 &&
           "apply_evolve: evolution card must be Stage 1 or higher");
    assert(evolution_card.evolves_from.has_value() &&
           evolution_card.evolves_from.value() == from.card.name &&
           "apply_evolve: evolution card does not evolve from the target Pokemon");
    assert(evolution_card.stage == from.card.stage + 1 &&
           "apply_evolve: stage must increase by exactly 1");

    // Build the new InPlayPokemon
    InPlayPokemon evolved;
    evolved.card             = evolution_card;
    evolved.damage_counters  = from.damage_counters;
    evolved.attached_energy  = from.attached_energy;
    evolved.attached_tool    = from.attached_tool;
    evolved.played_this_turn = false; // evolution does not count as "placed this turn"
    // status is default None — evolution cures volatile status conditions

    // Build cards_behind: old chain + the pre-evolution top card
    evolved.cards_behind = from.cards_behind;
    evolved.cards_behind.push_back(from.card);

    // Replace the slot
    target_slot = evolved;

    // Remove the evolution card from hand
    hand.erase(it);
}

// ---------------------------------------------------------------------------
// apply_ability_action
// ---------------------------------------------------------------------------

void apply_ability_action(GameState& gs, const Action& action, std::mt19937& rng)
{
    assert(action.type == ActionType::UseAbility &&
           "apply_ability_action: action type must be UseAbility");

    const int player   = gs.current_player;
    const int slot_idx = action.slot_index;

    assert(slot_idx >= 0 && slot_idx < 4 &&
           "apply_ability_action: slot_index out of range");

    auto& slot = gs.players[player].pokemon_slots[slot_idx];
    assert(slot.has_value() && "apply_ability_action: no Pokemon in slot");

    InPlayPokemon& ip = *slot;

    assert(!ip.ability_used_this_turn &&
           "apply_ability_action: ability already used this turn");

    // Get the AbilityMechanic directly from the card — resolved at load time
    const AbilityMechanic* mech = ip.card.ability.has_value()
        ? ip.card.ability->mechanic.get()
        : nullptr;

    // If no mechanic found, treat as UnknownAbilityMechanic (no-op)
    if (mech == nullptr)
    {
        ip.ability_used_this_turn = true;
        return;
    }

    assert(mech->timing() == AbilityTiming::Activate &&
           "apply_ability_action: cannot manually trigger a Passive ability");

    mech->apply_activate(gs, player, slot_idx, rng);

    // Mark ability as used this turn (slot may have changed if Pokemon was KO'd,
    // so re-fetch the slot pointer safely)
    auto& slot_after = gs.players[player].pokemon_slots[slot_idx];
    if (slot_after.has_value())
        slot_after->ability_used_this_turn = true;
}

} // namespace ptcgp_sim
