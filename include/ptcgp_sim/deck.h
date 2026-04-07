#pragma once

#include "card.h"
#include <vector>
#include <string>

namespace ptcgp_sim {

struct Deck {
    std::string       name;
    std::vector<Card> cards;

    bool is_valid() const; // must contain exactly 20 cards
};

} // namespace ptcgp_sim
