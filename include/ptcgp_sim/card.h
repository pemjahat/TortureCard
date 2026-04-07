#pragma once

#include <string>
#include <vector>

namespace ptcgp_sim {

enum class CardType {
    Pokemon,
    Trainer,
    Energy,
};

struct Card {
    std::string id;
    std::string name;
    CardType    type;
    int         hp{0};
};

} // namespace ptcgp_sim
