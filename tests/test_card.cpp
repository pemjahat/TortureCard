#include <gtest/gtest.h>
#include "ptcgp_sim/card.h"

using namespace ptcgp_sim;

TEST(CardTest, DefaultHpIsZero) {
    Card c;
    EXPECT_EQ(c.hp, 0);
}

TEST(CardTest, FieldsAssignedCorrectly) {
    Card c;
    c.id   = "A1-001";
    c.name = "Bulbasaur";
    c.type = CardType::Pokemon;
    c.hp   = 70;

    EXPECT_EQ(c.id,   "A1-001");
    EXPECT_EQ(c.name, "Bulbasaur");
    EXPECT_EQ(c.type, CardType::Pokemon);
    EXPECT_EQ(c.hp,   70);
}

TEST(CardTest, CardTypeEnumValues) {
    Card trainer;
    trainer.type = CardType::Trainer;
    EXPECT_EQ(trainer.type, CardType::Trainer);

    Card energy;
    energy.type = CardType::Energy;
    EXPECT_EQ(energy.type, CardType::Energy);
}
