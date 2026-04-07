#include <gtest/gtest.h>
#include "ptcgp_sim/deck.h"

using namespace ptcgp_sim;

// Helper: build a deck with N pokemon cards
static Deck make_deck(int count) {
    Deck d;
    d.name = "TestDeck";
    for (int i = 0; i < count; ++i) {
        Card c;
        c.id   = "T-" + std::to_string(i);
        c.name = "Pokemon" + std::to_string(i);
        c.type = CardType::Pokemon;
        c.hp   = 60;
        d.cards.push_back(c);
    }
    return d;
}

TEST(DeckTest, EmptyDeckIsInvalid) {
    Deck d;
    EXPECT_FALSE(d.is_valid());
}

TEST(DeckTest, Under20CardsIsInvalid) {
    Deck d = make_deck(19);
    EXPECT_FALSE(d.is_valid());
}

TEST(DeckTest, Exactly20CardsIsValid) {
    Deck d = make_deck(20);
    EXPECT_TRUE(d.is_valid());
}

TEST(DeckTest, Over20CardsIsInvalid) {
    Deck d = make_deck(21);
    EXPECT_FALSE(d.is_valid());
}

TEST(DeckTest, DeckNameIsPreserved) {
    Deck d = make_deck(20);
    EXPECT_EQ(d.name, "TestDeck");
}

TEST(DeckTest, CardCountMatchesAdded) {
    Deck d = make_deck(20);
    EXPECT_EQ(static_cast<int>(d.cards.size()), 20);
}
