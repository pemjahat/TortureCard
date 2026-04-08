#include "ptcgp_sim/deck.h"

namespace ptcgp_sim 
{

bool Deck::is_valid() const 
{
    return cards.size() == 20;
}

} // namespace ptcgp_sim
