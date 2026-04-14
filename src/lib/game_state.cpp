#include "ptcgp_sim/game_state.h"

#include <cassert>
#include <random>

// GameState, PlayerState, and InPlayPokemon are fully defined as inline
// structs/methods in game_state.h.  Out-of-line helper implementations live here.

namespace ptcgp_sim {

// ---------------------------------------------------------------------------
// GameState::deal_starting_hands
// ---------------------------------------------------------------------------

void GameState::deal_starting_hands(std::mt19937& rng)
{
    // Guard against double-dealing
    assert(players[0].hand.empty() && players[1].hand.empty()
           && "deal_starting_hands: players already have cards in hand");

    for (int p = 0; p < 2; ++p)
        players[p].hand = players[p].deck.deal_starting_hand(rng);
}

} // namespace ptcgp_sim
