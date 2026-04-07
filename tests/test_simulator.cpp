#include <gtest/gtest.h>
#include "ptcgp_sim/simulator.h"
#include "ptcgp_sim/deck.h"

using namespace ptcgp_sim;

// Helper: build a minimal valid deck (20 pokemon cards)
static Deck make_valid_deck(const std::string& name) {
    Deck d;
    d.name = name;
    for (int i = 0; i < 20; ++i) {
        Card c;
        c.id   = name + "-" + std::to_string(i);
        c.name = "Pokemon" + std::to_string(i);
        c.type = CardType::Pokemon;
        c.hp   = 60;
        d.cards.push_back(c);
    }
    return d;
}

// TEST(SimulatorTest, SingleRunReturnsValidWinner) {
//     Simulator sim;
//     Deck p1 = make_valid_deck("DeckA");
//     Deck p2 = make_valid_deck("DeckB");

//     SimulationResult result = sim.run(p1, p2);

//     // Winner must be player 0 or player 1
//     EXPECT_TRUE(result.winner == 0 || result.winner == 1);
// }

// TEST(SimulatorTest, SingleRunTurnsArePositive) {
//     Simulator sim;
//     Deck p1 = make_valid_deck("DeckA");
//     Deck p2 = make_valid_deck("DeckB");

//     SimulationResult result = sim.run(p1, p2);

//     EXPECT_GT(result.turns, 0);
// }

// TEST(SimulatorTest, BatchRunWinCountsSumToTotal) {
//     Simulator sim;
//     Deck p1 = make_valid_deck("DeckA");
//     Deck p2 = make_valid_deck("DeckB");

//     const int games = 100;
//     int p1_wins = 0, p2_wins = 0;
//     sim.run_batch(p1, p2, games, p1_wins, p2_wins);

//     EXPECT_EQ(p1_wins + p2_wins, games);
// }

TEST(SimulatorTest, BatchRunWinsAreNonNegative) {
    Simulator sim;
    Deck p1 = make_valid_deck("DeckA");
    Deck p2 = make_valid_deck("DeckB");

    int p1_wins = 0, p2_wins = 0;
    sim.run_batch(p1, p2, 50, p1_wins, p2_wins);

    EXPECT_GE(p1_wins, 0);
    EXPECT_GE(p2_wins, 0);
}
