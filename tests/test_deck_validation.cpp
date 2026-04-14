// Unit tests for Deck::validate().
// Uses try/catch-based testing — no external framework required.
// Build target: ptcgp_test_deck (added in CMakeLists.txt)

#include "ptcgp_sim/card.h"
#include "ptcgp_sim/database.h"
#include "ptcgp_sim/deck.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

// Macro that throws with file, line, and expression on failure.
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

// Build a Deck directly (no JSON / database needed) from a list of Cards.
static ptcgp_sim::Deck make_deck(const std::vector<ptcgp_sim::Card>& cards,
                                  const std::vector<ptcgp_sim::EnergyType>& energy_types)
{
    ptcgp_sim::Deck d;
    d.energy_types = energy_types;
    d.cards        = cards;

    // Build entries: one entry per unique card id, count = occurrences
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

static ptcgp_sim::Card make_card(const std::string& expansion, int number,
                                  const std::string& name)
{
    ptcgp_sim::Card c;
    c.id   = {expansion, number};
    c.name = name;
    c.type = ptcgp_sim::CardType::Pokemon;
    c.hp   = 60;
    return c;
}

// Produce N copies of the same card
static std::vector<ptcgp_sim::Card> repeat_card(const ptcgp_sim::Card& card, int n)
{
    return std::vector<ptcgp_sim::Card>(n, card);
}

// ---------------------------------------------------------------------------
// Test 1: Valid deck — 20 cards + energy type passes
// ---------------------------------------------------------------------------

static void test_valid_deck_passes()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");
    auto deck = make_deck(repeat_card(bulbasaur, 20), {EnergyType::Grass});

    std::vector<std::string> errors;
    bool result = deck.validate(errors);

    REQUIRE(result == true);
    REQUIRE(errors.empty());

    std::cout << "  [PASS] test_valid_deck_passes\n";
}

// ---------------------------------------------------------------------------
// Test 2: Too few cards — fails with descriptive error
// ---------------------------------------------------------------------------

static void test_too_few_cards_fails()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");
    auto deck = make_deck(repeat_card(bulbasaur, 15), {EnergyType::Grass});

    std::vector<std::string> errors;
    bool result = deck.validate(errors);

    REQUIRE(result == false);
    REQUIRE(!errors.empty());
    // Error message should mention the actual count
    bool mentions_count = std::any_of(errors.begin(), errors.end(),
        [](const std::string& e){ return e.find("15") != std::string::npos; });
    REQUIRE(mentions_count);

    std::cout << "  [PASS] test_too_few_cards_fails\n";
}

// ---------------------------------------------------------------------------
// Test 3: Too many cards — fails with descriptive error
// ---------------------------------------------------------------------------

static void test_too_many_cards_fails()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");
    auto deck = make_deck(repeat_card(bulbasaur, 25), {EnergyType::Fire});

    std::vector<std::string> errors;
    bool result = deck.validate(errors);

    REQUIRE(result == false);
    REQUIRE(!errors.empty());
    bool mentions_count = std::any_of(errors.begin(), errors.end(),
        [](const std::string& e){ return e.find("25") != std::string::npos; });
    REQUIRE(mentions_count);

    std::cout << "  [PASS] test_too_many_cards_fails\n";
}

// ---------------------------------------------------------------------------
// Test 4: Missing energy type — fails
// ---------------------------------------------------------------------------

static void test_missing_energy_type_fails()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");
    auto deck = make_deck(repeat_card(bulbasaur, 20), {}); // no energy types

    std::vector<std::string> errors;
    bool result = deck.validate(errors);

    REQUIRE(result == false);
    REQUIRE(!errors.empty());

    std::cout << "  [PASS] test_missing_energy_type_fails\n";
}

// ---------------------------------------------------------------------------
// Test 5: Both wrong count AND missing energy — both errors reported
// ---------------------------------------------------------------------------

static void test_multiple_errors_all_reported()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");
    auto deck = make_deck(repeat_card(bulbasaur, 10), {}); // 10 cards, no energy

    std::vector<std::string> errors;
    bool result = deck.validate(errors);

    REQUIRE(result == false);
    REQUIRE(errors.size() >= 2); // both errors must be present

    std::cout << "  [PASS] test_multiple_errors_all_reported\n";
}

// ---------------------------------------------------------------------------
// Test 6: is_valid() convenience method mirrors validate()
// ---------------------------------------------------------------------------

static void test_is_valid_mirrors_validate()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_card("A1", 1, "Bulbasaur");

    auto good_deck = make_deck(repeat_card(bulbasaur, 20), {EnergyType::Grass});
    auto bad_deck  = make_deck(repeat_card(bulbasaur, 5),  {});

    REQUIRE(good_deck.is_valid()  == true);
    REQUIRE(bad_deck.is_valid()   == false);

    std::cout << "  [PASS] test_is_valid_mirrors_validate\n";
}

// ---------------------------------------------------------------------------
// Test 7: total_cards() counts entries correctly
// ---------------------------------------------------------------------------

static void test_total_cards_counts_entries()
{
    using namespace ptcgp_sim;

    Card a = make_card("A1", 1,  "Bulbasaur");
    Card b = make_card("A1", 33, "Charmander");

    // 12 copies of a + 8 copies of b = 20
    std::vector<Card> cards;
    for (int i = 0; i < 12; ++i) cards.push_back(a);
    for (int i = 0; i < 8;  ++i) cards.push_back(b);

    auto deck = make_deck(cards, {EnergyType::Grass, EnergyType::Fire});

    REQUIRE(deck.total_cards() == 20);

    std::vector<std::string> errors;
    REQUIRE(deck.validate(errors) == true);

    std::cout << "  [PASS] test_total_cards_counts_entries\n";
}

// ---------------------------------------------------------------------------
// Test 8: Database loads successfully and contains cards
// ---------------------------------------------------------------------------

static void test_database_loads(ptcgp_sim::Database& out_db)
{
    out_db = ptcgp_sim::Database::load();

    REQUIRE(out_db.size() > 0);

    // Bulbasaur (A1 001) must exist
    ptcgp_sim::CardId bulbasaur_id{"A1", 1};
    const ptcgp_sim::Card* card = out_db.find_by_id(bulbasaur_id);
    REQUIRE(card != nullptr);
    REQUIRE(card->name == "Bulbasaur");
    REQUIRE(card->type == ptcgp_sim::CardType::Pokemon);

    std::cout << "  [PASS] test_database_loads (" << out_db.size() << " cards)\n";
}

// ---------------------------------------------------------------------------
// Test 9: Deck::load_from_json loads and validates a real deck from database
// ---------------------------------------------------------------------------

static void test_load_from_database(const ptcgp_sim::Database& db)
{
    ptcgp_sim::Deck deck = ptcgp_sim::Deck::load_from_json(PTCGP_TEST_DECK_PATH, db);

    // Deck should have exactly 20 cards
    REQUIRE(deck.total_cards() == 20);

    // Energy type should be Grass
    REQUIRE(!deck.energy_types.empty());
    REQUIRE(deck.energy_types[0] == ptcgp_sim::EnergyType::Grass);

    // All 20 cards should be Bulbasaur
    REQUIRE(deck.cards.size() == 20);
    for (const auto& c : deck.cards)
        REQUIRE(c.name == "Bulbasaur");

    // Full validation must pass
    std::vector<std::string> errors;
    bool valid = deck.validate(errors);
    REQUIRE(valid);
    REQUIRE(errors.empty());

    std::cout << "  [PASS] test_load_from_database\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim deck validation tests ===\n";

    // In-memory tests (no database required)
    RUN_TEST(test_valid_deck_passes);
    RUN_TEST(test_too_few_cards_fails);
    RUN_TEST(test_too_many_cards_fails);
    RUN_TEST(test_missing_energy_type_fails);
    RUN_TEST(test_multiple_errors_all_reported);
    RUN_TEST(test_is_valid_mirrors_validate);
    RUN_TEST(test_total_cards_counts_entries);

    // Database-loading tests (require database.json to be present)
    std::cout << "\n  Loading database from: " PTCGP_DATABASE_PATH "\n";
    ptcgp_sim::Database db;
    try {
        test_database_loads(db);
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] test_database_loads\n"
                  << "         " << e.what() << "\n";
        ++g_failures;
    }
    try {
        test_load_from_database(db);
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] test_load_from_database\n"
                  << "         " << e.what() << "\n";
        ++g_failures;
    }

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "All tests passed.\n";
    } else {
        std::cerr << g_failures << " test(s) FAILED.\n";
    }
    return g_failures > 0 ? 1 : 0;
}
