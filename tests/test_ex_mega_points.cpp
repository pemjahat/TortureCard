// Unit tests for Ex & Mega Pokemon knockout point logic:
//   - Requirement 1: is_ex() classification
//   - Requirement 2: is_mega() classification
//   - Requirement 3: knockout_points() helper
//   - Requirement 4: variable prize points in resolve_knockouts()
//   - Requirement 5: win-condition triggered by Ex/Mega KO
//
// Uses the same try/catch test infrastructure as the other test files.
// Build target: ptcgp_test_ex_mega (added in CMakeLists.txt)

#include "ptcgp_sim/action.h"
#include "ptcgp_sim/card.h"
#include "ptcgp_sim/deck.h"
#include "ptcgp_sim/effects.h"
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

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) {                                                         \
            throw std::runtime_error(                                          \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +       \
                " — REQUIRE failed: " #expr);                                  \
        }                                                                      \
    } while (false)

static int g_failures = 0;

#define RUN_TEST(func)                                                         \
    do {                                                                       \
        try {                                                                  \
            func();                                                            \
        } catch (const std::exception& e) {                                    \
            std::cerr << "  [FAIL] " #func "\n"                                \
                      << "         " << e.what() << "\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (false)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ptcgp_sim::Card make_pokemon(
    const std::string& expansion, int number,
    const std::string& name,
    int hp = 60,
    ptcgp_sim::EnergyType energy_type = ptcgp_sim::EnergyType::Colorless,
    int stage = 0,
    const std::vector<ptcgp_sim::Attack>& attacks = {})
{
    ptcgp_sim::Card c;
    c.id          = {expansion, number};
    c.name        = name;
    c.type        = ptcgp_sim::CardType::Pokemon;
    c.hp          = hp;
    c.energy_type = energy_type;
    c.stage       = stage;
    c.attacks     = attacks;
    return c;
}

static ptcgp_sim::Card make_trainer(
    const std::string& expansion, int number,
    const std::string& name)
{
    ptcgp_sim::Card c;
    c.id           = {expansion, number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Trainer;
    c.trainer_type = ptcgp_sim::TrainerType::Item;
    return c;
}

static ptcgp_sim::Attack make_attack(const std::string& name, int damage)
{
    ptcgp_sim::Attack a;
    a.name   = name;
    a.damage = damage;
    return a;
}

// Build a minimal 20-card deck (all copies of the same card)
static ptcgp_sim::Deck make_deck(const ptcgp_sim::Card& card)
{
    ptcgp_sim::Deck d;
    d.energy_types = {ptcgp_sim::EnergyType::Colorless};
    d.cards        = std::vector<ptcgp_sim::Card>(20, card);
    d.entries.push_back({card.id, 20});
    return d;
}

// Build a fresh GameState with both actives placed and turn in Action phase.
static ptcgp_sim::GameState make_game(
    const ptcgp_sim::Card& p0_active,
    const ptcgp_sim::Card& p1_active)
{
    using namespace ptcgp_sim;

    Card dummy = make_pokemon("XX", 99, "Dummy", 60);
    Deck deck  = make_deck(dummy);

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 2;
    gs.current_player = 0;

    InPlayPokemon ip0; ip0.card = p0_active; ip0.played_this_turn = false;
    InPlayPokemon ip1; ip1.card = p1_active; ip1.played_this_turn = false;
    gs.players[0].pokemon_slots[0] = ip0;
    gs.players[1].pokemon_slots[0] = ip1;

    return gs;
}

// ============================================================================
// REQUIREMENT 1: is_ex() classification
// ============================================================================

static void test_is_ex_true_for_ex_pokemon()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Mewtwo ex").is_ex()   == true);
    REQUIRE(make_pokemon("A1", 2, "Charizard ex").is_ex() == true);
    REQUIRE(make_pokemon("A1", 3, "Pikachu ex").is_ex()   == true);

    std::cout << "  [PASS] test_is_ex_true_for_ex_pokemon\n";
}

static void test_is_ex_false_for_regular_pokemon()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Pikachu").is_ex()   == false);
    REQUIRE(make_pokemon("A1", 2, "Charizard").is_ex() == false);
    REQUIRE(make_pokemon("A1", 3, "Mewtwo").is_ex()    == false);

    std::cout << "  [PASS] test_is_ex_false_for_regular_pokemon\n";
}

static void test_is_ex_false_for_trainer_card()
{
    using namespace ptcgp_sim;

    // Even if a trainer card somehow had "ex" in its name, is_ex() must return false
    Card trainer = make_trainer("A1", 10, "Trainer ex");
    REQUIRE(trainer.is_ex() == false);

    std::cout << "  [PASS] test_is_ex_false_for_trainer_card\n";
}

static void test_is_ex_false_for_name_exactly_ex()
{
    using namespace ptcgp_sim;

    // Name is exactly "ex" — no preceding word, must return false
    REQUIRE(make_pokemon("A1", 1, "ex").is_ex() == false);

    std::cout << "  [PASS] test_is_ex_false_for_name_exactly_ex\n";
}

// ============================================================================
// REQUIREMENT 2: is_mega() classification
// ============================================================================

static void test_is_mega_true_for_mega_pokemon()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Mega Charizard").is_mega() == true);
    REQUIRE(make_pokemon("A1", 2, "Mega Mewtwo").is_mega()    == true);

    std::cout << "  [PASS] test_is_mega_true_for_mega_pokemon\n";
}

static void test_is_mega_false_for_regular_pokemon()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Charizard").is_mega()  == false);
    REQUIRE(make_pokemon("A1", 2, "Mewtwo ex").is_mega()  == false);

    std::cout << "  [PASS] test_is_mega_false_for_regular_pokemon\n";
}

static void test_is_mega_false_for_trainer_card()
{
    using namespace ptcgp_sim;

    Card trainer = make_trainer("A1", 10, "Mega Potion");
    REQUIRE(trainer.is_mega() == false);

    std::cout << "  [PASS] test_is_mega_false_for_trainer_card\n";
}

// ============================================================================
// REQUIREMENT 3: knockout_points() helper
// ============================================================================

static void test_knockout_points_regular_returns_1()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Pikachu").knockout_points()   == 1);
    REQUIRE(make_pokemon("A1", 2, "Charizard").knockout_points() == 1);

    std::cout << "  [PASS] test_knockout_points_regular_returns_1\n";
}

static void test_knockout_points_ex_returns_2()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Mewtwo ex").knockout_points()   == 2);
    REQUIRE(make_pokemon("A1", 2, "Charizard ex").knockout_points() == 2);

    std::cout << "  [PASS] test_knockout_points_ex_returns_2\n";
}

static void test_knockout_points_mega_returns_3()
{
    using namespace ptcgp_sim;

    REQUIRE(make_pokemon("A1", 1, "Mega Charizard").knockout_points() == 3);
    REQUIRE(make_pokemon("A1", 2, "Mega Mewtwo").knockout_points()    == 3);

    std::cout << "  [PASS] test_knockout_points_mega_returns_3\n";
}

static void test_knockout_points_mega_ex_returns_3()
{
    using namespace ptcgp_sim;

    // Mega takes priority over ex
    REQUIRE(make_pokemon("A1", 1, "Mega Charizard ex").knockout_points() == 3);

    std::cout << "  [PASS] test_knockout_points_mega_ex_returns_3\n";
}

static void test_knockout_points_trainer_returns_0()
{
    using namespace ptcgp_sim;

    REQUIRE(make_trainer("A1", 10, "Poke Ball").knockout_points() == 0);

    std::cout << "  [PASS] test_knockout_points_trainer_returns_0\n";
}

// ============================================================================
// REQUIREMENT 4: Variable prize points in resolve_knockouts()
// ============================================================================

static void test_ko_regular_awards_1_point()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60); // regular

    GameState gs = make_game(attacker, defender);
    // Give defender a bench so game doesn't end on no-bench path
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 1);

    std::cout << "  [PASS] test_ko_regular_awards_1_point\n";
}

static void test_ko_ex_awards_2_points()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 200);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender_ex = make_pokemon("A1", 2, "Mewtwo ex", 150); // ex Pokemon

    GameState gs = make_game(attacker, defender_ex);
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 2);

    std::cout << "  [PASS] test_ko_ex_awards_2_points\n";
}

static void test_ko_mega_awards_3_points()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 300);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender_mega = make_pokemon("A1", 2, "Mega Charizard", 220); // Mega Pokemon

    GameState gs = make_game(attacker, defender_mega);
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 3);

    std::cout << "  [PASS] test_ko_mega_awards_3_points\n";
}

// ============================================================================
// REQUIREMENT 5: Win condition triggered by Ex/Mega KO
// ============================================================================

static void test_ko_ex_triggers_game_over_at_3_points()
{
    using namespace ptcgp_sim;

    // Player 0 already has 1 point; knocking out an ex (2 pts) reaches 3 -> win
    Attack ko_punch = make_attack("KO Punch", 200);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender_ex = make_pokemon("A1", 2, "Charizard ex", 150);

    GameState gs = make_game(attacker, defender_ex);
    gs.players[0].points = 1; // one more ex KO wins
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 3);
    REQUIRE(gs.game_over == true);
    REQUIRE(gs.winner == 0);

    std::cout << "  [PASS] test_ko_ex_triggers_game_over_at_3_points\n";
}

static void test_ko_mega_triggers_immediate_game_over()
{
    using namespace ptcgp_sim;

    // Player 0 has 0 points; knocking out a Mega (3 pts) wins immediately
    Attack ko_punch = make_attack("KO Punch", 300);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender_mega = make_pokemon("A1", 2, "Mega Mewtwo", 220);

    GameState gs = make_game(attacker, defender_mega);
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 3);
    REQUIRE(gs.game_over == true);
    REQUIRE(gs.winner == 0);

    std::cout << "  [PASS] test_ko_mega_triggers_immediate_game_over\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== ptcgp_sim ex/mega knockout point tests ===\n";

    // Requirement 1: is_ex()
    RUN_TEST(test_is_ex_true_for_ex_pokemon);
    RUN_TEST(test_is_ex_false_for_regular_pokemon);
    RUN_TEST(test_is_ex_false_for_trainer_card);
    RUN_TEST(test_is_ex_false_for_name_exactly_ex);

    // Requirement 2: is_mega()
    RUN_TEST(test_is_mega_true_for_mega_pokemon);
    RUN_TEST(test_is_mega_false_for_regular_pokemon);
    RUN_TEST(test_is_mega_false_for_trainer_card);

    // Requirement 3: knockout_points()
    RUN_TEST(test_knockout_points_regular_returns_1);
    RUN_TEST(test_knockout_points_ex_returns_2);
    RUN_TEST(test_knockout_points_mega_returns_3);
    RUN_TEST(test_knockout_points_mega_ex_returns_3);
    RUN_TEST(test_knockout_points_trainer_returns_0);

    // Requirement 4: variable prize points
    RUN_TEST(test_ko_regular_awards_1_point);
    RUN_TEST(test_ko_ex_awards_2_points);
    RUN_TEST(test_ko_mega_awards_3_points);

    // Requirement 5: win condition
    RUN_TEST(test_ko_ex_triggers_game_over_at_3_points);
    RUN_TEST(test_ko_mega_triggers_immediate_game_over);

    std::cout << "\n";
    if (g_failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cerr << g_failures << " test(s) FAILED.\n";

    return g_failures > 0 ? 1 : 0;
}
