#pragma once

#include "card.h"
#include "deck.h"
#include <vector>

namespace ptcgp_sim 
{

struct PlayerState 
{
    Deck              deck;
    std::vector<Card> hand;
    std::vector<Card> bench;
    Card*             active{nullptr};
    int               prize_cards_taken{0};
};

struct GameState 
{
    PlayerState players[2];
    int         turn{0};
    int         active_player{0}; // 0 or 1
    bool        game_over{false};
    int         winner{-1};       // -1 = no winner yet
};

} // namespace ptcgp_sim
