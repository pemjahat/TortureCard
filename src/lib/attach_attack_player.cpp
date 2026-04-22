#include "ptcgp_sim/attach_attack_player.h"
#include "ptcgp_sim/move_generation.h"

#include <algorithm>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// AttachAttackPlayer::active_can_attack
// ---------------------------------------------------------------------------

bool AttachAttackPlayer::active_can_attack(const GameState& gs, int player)
{
    const auto& active_slot = gs.players[player].pokemon_slots[0];
    if (!active_slot.has_value()) return false;

    const InPlayPokemon& active = *active_slot;
    for (const Attack& atk : active.card.attacks)
    {
        if (energy_satisfies_cost(active.attached_energy, atk.energy_required))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// AttachAttackPlayer::decide
// ---------------------------------------------------------------------------

Action AttachAttackPlayer::decide(const GameState& gs,
                                  const std::vector<Action>& legal_moves)
{
    const int player = gs.current_player;

    // If the active Pokemon can already attack, prefer Attack over attaching
    // more energy to it.
    if (active_can_attack(gs, player))
    {
        auto it = std::find_if(legal_moves.begin(), legal_moves.end(),
            [](const Action& a){ return a.type == ActionType::Attack; });
        if (it != legal_moves.end())
            return *it;
    }

    // Otherwise, prefer attaching energy to the active Pokemon (slot 0).
    {
        auto it = std::find_if(legal_moves.begin(), legal_moves.end(),
            [](const Action& a)
            {
                return a.type == ActionType::AttachEnergy && a.target_slot == 0;
            });
        if (it != legal_moves.end())
            return *it;
    }

    // If we can't attach to active, try to attack anyway.
    {
        auto it = std::find_if(legal_moves.begin(), legal_moves.end(),
            [](const Action& a){ return a.type == ActionType::Attack; });
        if (it != legal_moves.end())
            return *it;
    }

    // Fallback: first available legal move (e.g. Pass, PlayPokemon, etc.)
    return legal_moves.front();
}

} // namespace ptcgp_sim
