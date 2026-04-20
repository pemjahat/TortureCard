#include "ptcgp_sim/action.h"
#include <sstream>

namespace ptcgp_sim
{

std::string Action::to_string() const
{
    std::ostringstream oss;
    switch (type)
    {
        case ActionType::Draw:
            oss << "Draw";
            break;
        case ActionType::PlayPokemon:
            oss << "PlayPokemon(" << card_id.to_string() << ", slot=" << slot_index << ")";
            break;
        case ActionType::AttachEnergy:
            oss << "AttachEnergy(slot=" << target_slot << ")";
            break;
        case ActionType::Attack:
            oss << "Attack(" << attack_index << ")";
            break;
        case ActionType::Retreat:
            oss << "Retreat(to_slot=" << slot_index << ")";
            break;
        case ActionType::Pass:
            oss << "Pass";
            break;
        case ActionType::PlaySupporter:
            oss << "PlaySupporter(" << card_id.to_string() << ")";
            break;
        case ActionType::PlayItem:
            oss << "PlayItem(" << card_id.to_string() << ")";
            break;
        case ActionType::PlayTool:
            oss << "PlayTool(" << card_id.to_string() << ", slot=" << slot_index << ")";
            break;
        case ActionType::PlayStadium:
            oss << "PlayStadium(" << card_id.to_string() << ")";
            break;
        case ActionType::Evolve:
            oss << "Evolve(" << card_id.to_string() << ", slot=" << slot_index << ")";
            break;
        case ActionType::UseAbility:
            oss << "UseAbility(slot=" << slot_index << ")";
            break;
        default:
            oss << "Unknown";
            break;
    }
    return oss.str();
}

} // namespace ptcgp_sim
