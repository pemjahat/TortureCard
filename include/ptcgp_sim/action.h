#pragma once

#include "card.h"
#include <string>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Action — every legal move a player can make during their turn.
// Each variant carries only the data needed to apply it.
// ---------------------------------------------------------------------------
enum class ActionType
{
    Draw,           // Draw a card from the deck
    PlayPokemon,    // Place a Basic Pokemon from hand onto an empty slot
    AttachEnergy,   // Attach the turn's generated energy to a Pokemon in play
    Attack,         // Use the active Pokemon's attack at the given index
    Retreat,        // Swap the active Pokemon with a bench Pokemon
    Pass,           // End the current player's turn
    PlaySupporter,  // Play a Supporter card from hand (once per turn)
    PlayItem,       // Play an Item card from hand (no per-turn limit)
    PlayTool,       // Attach a Tool card to a Pokemon in play
    PlayStadium,    // Play a Stadium card (replaces current Stadium)
    Evolve,         // Evolve an in-play Pokemon using a card from hand
};

struct Action
{
    ActionType type{ActionType::Pass};

    // PlayPokemon
    CardId card_id{};       // card to play (PlayPokemon, PlaySupporter, PlayItem, PlayTool, PlayStadium)
    int    slot_index{-1};  // target slot (PlayPokemon: 0-3, PlayTool: 0-3, Retreat: 1-3)

    // AttachEnergy
    EnergyType energy_type{EnergyType::Colorless}; // energy type to attach
    int        target_slot{-1};                    // slot to attach energy to

    // Attack
    int attack_index{-1};  // index into active Pokemon's attacks list

    // ---------------------------------------------------------------------------
    // Factory helpers — construct a correctly-filled Action for each type
    // ---------------------------------------------------------------------------
    static Action draw()
    {
        Action a; a.type = ActionType::Draw; return a;
    }

    static Action play_pokemon(const CardId& id, int slot)
    {
        Action a; a.type = ActionType::PlayPokemon; a.card_id = id; a.slot_index = slot; return a;
    }

    static Action attach_energy(EnergyType e, int slot)
    {
        Action a; a.type = ActionType::AttachEnergy; a.energy_type = e; a.target_slot = slot; return a;
    }

    static Action attack(int index)
    {
        Action a; a.type = ActionType::Attack; a.attack_index = index; return a;
    }

    static Action retreat(int to_slot)
    {
        Action a; a.type = ActionType::Retreat; a.slot_index = to_slot; return a;
    }

    static Action pass()
    {
        Action a; a.type = ActionType::Pass; return a;
    }

    static Action play_supporter(const CardId& id)
    {
        Action a; a.type = ActionType::PlaySupporter; a.card_id = id; return a;
    }

    static Action play_item(const CardId& id)
    {
        Action a; a.type = ActionType::PlayItem; a.card_id = id; return a;
    }

    static Action play_tool(const CardId& id, int target_slot)
    {
        Action a; a.type = ActionType::PlayTool; a.card_id = id; a.slot_index = target_slot; return a;
    }

    static Action play_stadium(const CardId& id)
    {
        Action a; a.type = ActionType::PlayStadium; a.card_id = id; return a;
    }

    static Action evolve(const CardId& id, int slot)
    {
        Action a; a.type = ActionType::Evolve; a.card_id = id; a.slot_index = slot; return a;
    }

    // Human-readable description for CLI output and debugging
    std::string to_string() const;
};

} // namespace ptcgp_sim
