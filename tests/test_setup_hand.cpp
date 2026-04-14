// Unit tests for Deck::shuffle, Deck::deal_starting_hand, and
// GameState::deal_starting_hands.
// Uses try/catch-based testing — no external framework required.
// Build target: ptcgp_test_setup_hand (added in CMakeLists.txt)

#include "ptcgp_sim/card.h"
#include "ptcgp_sim/deck.h"
#include "ptcgp_sim/game_state.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

#define REQUIRE(expr)                                                         \
    do {                                                                      \
        if (!(expr)) {                                                        \
            throw std::runtime_error(                                         \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                " — REQUIRE failed: " #expr);                                 \
        }                                                                     \
    } while (false)

static int g_failures = 0;

#define RUN_TEST(func)                                                        \
    do {                                                                      \
        try {                                                                 \
            func();                                                           \
        } catch (const std::exception& e) {                                   \
            std::cerr << "  [FAIL] " #func "\n"                               \
                      << "         " << e.what() << "\n";                     \
            ++g_failures;                                                     \
        }                                                                     \
    } while (false)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ptcgp_sim::Card make_pokemon(const std::string& expansion, int number,
                                    const std::string& name, int stage = 0)
{
    ptcgp_sim::Card c;
    c.id    = {expansion, number};
    c.name  = name;
    c.type  = ptcgp_sim::CardType::Pokemon;
    c.hp    = 60;
    c.stage = stage;
    return c;
}

static ptcgp_sim::Card make_trainer(const std::string& expansion, int number,
                                    const std::string& name)
{
    ptcgp_sim::Card c;
    c.id           = {expansion, number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Trainer;
    c.trainer_type = ptcgp_sim::TrainerType::Item;
    return c;
}

// Build a 20-card Deck from a vector of cards (no database needed).
static ptcgp_sim::Deck make_deck(const std::vector<ptcgp_sim::Card>& cards)
{
    ptcgp_sim::Deck d;
    d.energy_types = {ptcgp_sim::EnergyType::Fire};
    d.cards        = cards;
    for (const auto& c : cards)
    {
        auto it = std::find_if(d.entries.begin(), d.entries.end(),
                               [&](const ptcgp_sim::DeckEntry& e){ return e.id == c.id; });
        if (it != d.entries.end())
            it->count++;
        else
            d.entries.push_back({c.id, 1});
    }
    return d;
}

static bool has_stage0(const std::vector<ptcgp_sim::Card>& hand)
{
    return std::any_of(hand.begin(), hand.end(),
        [](const ptcgp_sim::Card& c){
            return c.type == ptcgp_sim::CardType::Pokemon && c.stage == 0;
        });
}

// ---------------------------------------------------------------------------
// Test 1: Deck::shuffle preserves deck size
// ---------------------------------------------------------------------------

static void test_shuffle_preserves_size()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur");
    std::vector<Card> cards(20, bulbasaur);
    Deck deck = make_deck(cards);

    std::mt19937 rng(42);
    deck.shuffle(rng);

    REQUIRE(deck.cards.size() == 20);

    std::cout << "  [PASS] test_shuffle_preserves_size\n";
}

// ---------------------------------------------------------------------------
// Test 2: Deck::shuffle is deterministic with a fixed seed
// ---------------------------------------------------------------------------

static void test_shuffle_deterministic_with_seed()
{
    using namespace ptcgp_sim;

    // Build a deck with distinguishable cards
    std::vector<Card> cards;
    for (int i = 0; i < 20; ++i)
        cards.push_back(make_pokemon("A1", i + 1, "Pokemon" + std::to_string(i)));

    Deck deck_a = make_deck(cards);
    Deck deck_b = make_deck(cards);

    std::mt19937 rng_a(12345);
    std::mt19937 rng_b(12345);
    deck_a.shuffle(rng_a);
    deck_b.shuffle(rng_b);

    REQUIRE(deck_a.cards.size() == deck_b.cards.size());
    for (std::size_t i = 0; i < deck_a.cards.size(); ++i)
        REQUIRE(deck_a.cards[i].id == deck_b.cards[i].id);

    std::cout << "  [PASS] test_shuffle_deterministic_with_seed\n";
}

// ---------------------------------------------------------------------------
// Test 3: deal_starting_hand returns exactly 5 cards
// ---------------------------------------------------------------------------

static void test_deal_returns_5_cards()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur");
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    std::mt19937 rng(99);
    auto hand = deck.deal_starting_hand(rng);

    REQUIRE(hand.size() == 5);

    std::cout << "  [PASS] test_deal_returns_5_cards\n";
}

// ---------------------------------------------------------------------------
// Test 4: deal_starting_hand leaves 15 cards in the deck
// ---------------------------------------------------------------------------

static void test_deal_leaves_15_in_deck()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur");
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    std::mt19937 rng(7);
    deck.deal_starting_hand(rng);

    REQUIRE(deck.cards.size() == 15);

    std::cout << "  [PASS] test_deal_leaves_15_in_deck\n";
}

// ---------------------------------------------------------------------------
// Test 5: deal_starting_hand guarantees at least one stage-0 Pokemon
//         (deck is all stage-0 Pokemon — trivial case)
// ---------------------------------------------------------------------------

static void test_deal_hand_has_basic_all_pokemon_deck()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 0);
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    std::mt19937 rng(1);
    auto hand = deck.deal_starting_hand(rng);

    REQUIRE(has_stage0(hand));

    std::cout << "  [PASS] test_deal_hand_has_basic_all_pokemon_deck\n";
}

// ---------------------------------------------------------------------------
// Test 6: deal_starting_hand reshuffles when first 5 cards are all Trainers
//         (stage-0 Pokemon exist but are placed after position 5 initially)
// ---------------------------------------------------------------------------

static void test_deal_reshuffles_when_first5_are_trainers()
{
    using namespace ptcgp_sim;

    // Build a deck: 15 Trainers followed by 5 Bulbasaurs
    // With a fixed seed the shuffle will eventually put a Basic in the first 5.
    std::vector<Card> cards;
    for (int i = 0; i < 15; ++i)
        cards.push_back(make_trainer("P-A", i + 1, "Potion" + std::to_string(i)));
    for (int i = 0; i < 5; ++i)
        cards.push_back(make_pokemon("A1", i + 1, "Bulbasaur" + std::to_string(i), 0));

    Deck deck = make_deck(cards);

    // Force the deck into the "bad" order (all trainers first)
    // by not shuffling before calling deal_starting_hand.
    // deal_starting_hand will shuffle internally until a Basic appears.
    std::mt19937 rng(42);
    auto hand = deck.deal_starting_hand(rng);

    REQUIRE(hand.size() == 5);
    REQUIRE(has_stage0(hand));

    std::cout << "  [PASS] test_deal_reshuffles_when_first5_are_trainers\n";
}

// ---------------------------------------------------------------------------
// Test 7: deal_starting_hand places a stage-0 Pokemon as the first card
// ---------------------------------------------------------------------------

static void test_deal_first_card_is_basic()
{
    using namespace ptcgp_sim;

    // Mix of Trainers and Basics
    std::vector<Card> cards;
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_trainer("P-A", i + 1, "Item" + std::to_string(i)));
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_pokemon("A1", i + 1, "Pokemon" + std::to_string(i), 0));

    Deck deck = make_deck(cards);

    std::mt19937 rng(55);
    auto hand = deck.deal_starting_hand(rng);

    REQUIRE(hand[0].type == CardType::Pokemon);
    REQUIRE(hand[0].stage == 0);

    std::cout << "  [PASS] test_deal_first_card_is_basic\n";
}

// ---------------------------------------------------------------------------
// Test 8: deal_starting_hand is deterministic with a fixed seed
// ---------------------------------------------------------------------------

static void test_deal_deterministic_with_seed()
{
    using namespace ptcgp_sim;

    std::vector<Card> cards;
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_trainer("P-A", i + 1, "Item" + std::to_string(i)));
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_pokemon("A1", i + 1, "Pokemon" + std::to_string(i), 0));

    Deck deck_a = make_deck(cards);
    Deck deck_b = make_deck(cards);

    std::mt19937 rng_a(999);
    std::mt19937 rng_b(999);

    auto hand_a = deck_a.deal_starting_hand(rng_a);
    auto hand_b = deck_b.deal_starting_hand(rng_b);

    REQUIRE(hand_a.size() == hand_b.size());
    for (std::size_t i = 0; i < hand_a.size(); ++i)
        REQUIRE(hand_a[i].id == hand_b[i].id);

    std::cout << "  [PASS] test_deal_deterministic_with_seed\n";
}

// ---------------------------------------------------------------------------
// Test 9: GameState::deal_starting_hands gives both players 5 cards
// ---------------------------------------------------------------------------

static void test_gamestate_deal_both_players_5_cards()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 0);
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    GameState gs = GameState::make(deck, deck);

    std::mt19937 rng(0);
    gs.deal_starting_hands(rng);

    REQUIRE(gs.players[0].hand.size() == 5);
    REQUIRE(gs.players[1].hand.size() == 5);

    std::cout << "  [PASS] test_gamestate_deal_both_players_5_cards\n";
}

// ---------------------------------------------------------------------------
// Test 10: GameState::deal_starting_hands leaves 15 cards in each deck
// ---------------------------------------------------------------------------

static void test_gamestate_deal_leaves_15_in_each_deck()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 0);
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    GameState gs = GameState::make(deck, deck);

    std::mt19937 rng(0);
    gs.deal_starting_hands(rng);

    REQUIRE(gs.players[0].deck.cards.size() == 15);
    REQUIRE(gs.players[1].deck.cards.size() == 15);

    std::cout << "  [PASS] test_gamestate_deal_leaves_15_in_each_deck\n";
}

// ---------------------------------------------------------------------------
// Test 11: GameState::deal_starting_hands — both hands contain a stage-0 Pokemon
// ---------------------------------------------------------------------------

static void test_gamestate_deal_both_hands_have_basic()
{
    using namespace ptcgp_sim;

    // Mix: 10 Trainers + 10 Basics
    std::vector<Card> cards;
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_trainer("P-A", i + 1, "Item" + std::to_string(i)));
    for (int i = 0; i < 10; ++i)
        cards.push_back(make_pokemon("A1", i + 1, "Pokemon" + std::to_string(i), 0));

    Deck deck = make_deck(cards);

    GameState gs = GameState::make(deck, deck);

    std::mt19937 rng(77);
    gs.deal_starting_hands(rng);

    REQUIRE(has_stage0(gs.players[0].hand));
    REQUIRE(has_stage0(gs.players[1].hand));

    std::cout << "  [PASS] test_gamestate_deal_both_hands_have_basic\n";
}

// ---------------------------------------------------------------------------
// Test 12: GameState::deal_starting_hands does not modify other state fields
// ---------------------------------------------------------------------------

static void test_gamestate_deal_does_not_modify_other_fields()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 0);
    Deck deck = make_deck(std::vector<Card>(20, bulbasaur));

    GameState gs = GameState::make(deck, deck);

    std::mt19937 rng(3);
    gs.deal_starting_hands(rng);

    // Slots, discard piles, and points must be untouched
    for (int p = 0; p < 2; ++p)
    {
        REQUIRE(gs.players[p].points == 0);
        REQUIRE(gs.players[p].discard_pile.empty());
        for (int s = 0; s < 4; ++s)
            REQUIRE(!gs.players[p].pokemon_slots[s].has_value());
    }

    std::cout << "  [PASS] test_gamestate_deal_does_not_modify_other_fields\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim setup hand generation tests ===\n";

    RUN_TEST(test_shuffle_preserves_size);
    RUN_TEST(test_shuffle_deterministic_with_seed);
    RUN_TEST(test_deal_returns_5_cards);
    RUN_TEST(test_deal_leaves_15_in_deck);
    RUN_TEST(test_deal_hand_has_basic_all_pokemon_deck);
    RUN_TEST(test_deal_reshuffles_when_first5_are_trainers);
    RUN_TEST(test_deal_first_card_is_basic);
    RUN_TEST(test_deal_deterministic_with_seed);
    RUN_TEST(test_gamestate_deal_both_players_5_cards);
    RUN_TEST(test_gamestate_deal_leaves_15_in_each_deck);
    RUN_TEST(test_gamestate_deal_both_hands_have_basic);
    RUN_TEST(test_gamestate_deal_does_not_modify_other_fields);

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "All tests passed.\n";
    } else {
        std::cerr << g_failures << " test(s) FAILED.\n";
    }
    return g_failures > 0 ? 1 : 0;
}
