// Unit tests for Pokemon evolution feature.
// Covers: move generation rules, apply_evolve state mutations,
//         cards_behind chain, and knockout discard of full evolution chain.
// Build target: ptcgp_test_evolution (added in CMakeLists.txt)

#include "ptcgp_sim/action.h"
#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/move_generation.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Test infrastructure (same pattern as other test files)
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

static ptcgp_sim::Card make_basic(const std::string& name, int number = 1)
{
    ptcgp_sim::Card c;
    c.id    = {"TEST", number};
    c.name  = name;
    c.type  = ptcgp_sim::CardType::Pokemon;
    c.hp    = 60;
    c.stage = 0;
    return c;
}

static ptcgp_sim::Card make_stage1(const std::string& name,
                                    const std::string& evolves_from,
                                    int number = 2)
{
    ptcgp_sim::Card c;
    c.id           = {"TEST", number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Pokemon;
    c.hp           = 90;
    c.stage        = 1;
    c.evolves_from = evolves_from;
    return c;
}

static ptcgp_sim::Card make_stage2(const std::string& name,
                                    const std::string& evolves_from,
                                    int number = 3)
{
    ptcgp_sim::Card c;
    c.id           = {"TEST", number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Pokemon;
    c.hp           = 130;
    c.stage        = 2;
    c.evolves_from = evolves_from;
    return c;
}

static ptcgp_sim::InPlayPokemon make_in_play(const ptcgp_sim::Card& card,
                                              bool played_this_turn = false)
{
    ptcgp_sim::InPlayPokemon ip;
    ip.card             = card;
    ip.played_this_turn = played_this_turn;
    return ip;
}

static bool has_evolve_for(const std::vector<ptcgp_sim::Action>& moves,
                            const ptcgp_sim::CardId& card_id, int slot)
{
    return std::any_of(moves.begin(), moves.end(),
        [&](const ptcgp_sim::Action& a)
        {
            return a.type == ptcgp_sim::ActionType::Evolve &&
                   a.card_id == card_id &&
                   a.slot_index == slot;
        });
}

static bool has_action_type(const std::vector<ptcgp_sim::Action>& moves,
                             ptcgp_sim::ActionType t)
{
    return std::any_of(moves.begin(), moves.end(),
                       [t](const ptcgp_sim::Action& a){ return a.type == t; });
}

// ---------------------------------------------------------------------------
// Test 1 (AC 7.1): Evolve action generated when conditions are met
// ---------------------------------------------------------------------------

static void test_evolve_action_generated_when_valid()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 3; // not first turn for either player
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    // Bulbasaur in active slot, not placed this turn
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    // Ivysaur in hand
    gs.players[0].hand.push_back(ivysaur);

    auto moves = generate_legal_moves(gs, 0);

    REQUIRE(has_evolve_for(moves, ivysaur.id, 0));

    std::cout << "  [PASS] test_evolve_action_generated_when_valid\n";
}

// ---------------------------------------------------------------------------
// Test 2 (AC 7.2): Evolve NOT generated when target was placed this turn
// ---------------------------------------------------------------------------

static void test_evolve_blocked_when_played_this_turn()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 3;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    // Bulbasaur placed THIS turn
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, true);
    gs.players[0].hand.push_back(ivysaur);

    auto moves = generate_legal_moves(gs, 0);

    REQUIRE(!has_evolve_for(moves, ivysaur.id, 0));

    std::cout << "  [PASS] test_evolve_blocked_when_played_this_turn\n";
}

// ---------------------------------------------------------------------------
// Test 3 (AC 7.3): No Evolve actions on player's first turn
// ---------------------------------------------------------------------------

static void test_no_evolve_on_first_turn_player0()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 1; // player 0's first turn
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);

    auto moves = generate_legal_moves(gs, 0);

    REQUIRE(!has_action_type(moves, ActionType::Evolve));

    std::cout << "  [PASS] test_no_evolve_on_first_turn_player0\n";
}

static void test_no_evolve_on_first_turn_player1()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 2; // player 1's first turn
    gs.current_player = 1;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    gs.players[1].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[1].hand.push_back(ivysaur);

    auto moves = generate_legal_moves(gs, 1);

    REQUIRE(!has_action_type(moves, ActionType::Evolve));

    std::cout << "  [PASS] test_no_evolve_on_first_turn_player1\n";
}

// ---------------------------------------------------------------------------
// Test 4 (AC 7.4): apply_evolve removes card from hand
// ---------------------------------------------------------------------------

static void test_apply_evolve_removes_card_from_hand()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);

    apply_evolve(gs, 0, ivysaur.id, 0);

    // Ivysaur should no longer be in hand
    bool still_in_hand = std::any_of(gs.players[0].hand.begin(),
                                      gs.players[0].hand.end(),
                                      [&](const Card& c){ return c.id == ivysaur.id; });
    REQUIRE(!still_in_hand);

    std::cout << "  [PASS] test_apply_evolve_removes_card_from_hand\n";
}

// ---------------------------------------------------------------------------
// Test 5 (AC 7.5): apply_evolve preserves energy, tool, and damage counters
// ---------------------------------------------------------------------------

static void test_apply_evolve_preserves_energy_tool_damage()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    // Tool card
    Card rocky_helmet;
    rocky_helmet.id           = {"TEST", 99};
    rocky_helmet.name         = "Rocky Helmet";
    rocky_helmet.type         = CardType::Trainer;
    rocky_helmet.trainer_type = TrainerType::Tool;

    InPlayPokemon ip = make_in_play(bulbasaur, false);
    ip.attached_energy.push_back(EnergyType::Grass);
    ip.attached_energy.push_back(EnergyType::Colorless);
    ip.attached_tool    = rocky_helmet;
    ip.damage_counters  = 20;

    gs.players[0].pokemon_slots[0] = ip;
    gs.players[0].hand.push_back(ivysaur);

    apply_evolve(gs, 0, ivysaur.id, 0);

    const InPlayPokemon& evolved = *gs.players[0].pokemon_slots[0];
    REQUIRE(evolved.card.id == ivysaur.id);
    REQUIRE(evolved.damage_counters == 20);
    REQUIRE(evolved.attached_energy.size() == 2);
    REQUIRE(evolved.attached_tool.has_value());
    REQUIRE(evolved.attached_tool->id == rocky_helmet.id);

    std::cout << "  [PASS] test_apply_evolve_preserves_energy_tool_damage\n";
}

// ---------------------------------------------------------------------------
// Test 6 (AC 7.6): apply_evolve builds cards_behind correctly
// ---------------------------------------------------------------------------

static void test_apply_evolve_builds_cards_behind()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);

    apply_evolve(gs, 0, ivysaur.id, 0);

    const InPlayPokemon& evolved = *gs.players[0].pokemon_slots[0];
    REQUIRE(evolved.card.id == ivysaur.id);
    REQUIRE(evolved.cards_behind.size() == 1);
    REQUIRE(evolved.cards_behind[0].id == bulbasaur.id);

    std::cout << "  [PASS] test_apply_evolve_builds_cards_behind\n";
}

// ---------------------------------------------------------------------------
// Test 7 (AC 7.7): KO of Stage 1 discards both Stage 1 and Basic
// ---------------------------------------------------------------------------

static void test_ko_stage1_discards_full_chain()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    // Set up evolved Pokemon in active slot
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);
    apply_evolve(gs, 0, ivysaur.id, 0);

    // Give opponent an active Pokemon so the game doesn't end immediately
    Card charmander = make_basic("Charmander", 10);
    gs.players[1].pokemon_slots[0] = make_in_play(charmander, false);
    // Give player 0 a bench Pokemon so they don't lose on KO
    Card squirtle = make_basic("Squirtle", 11);
    gs.players[0].pokemon_slots[1] = make_in_play(squirtle, false);

    // Deal lethal damage to the evolved Ivysaur
    gs.players[0].pokemon_slots[0]->damage_counters = 999;

    resolve_knockouts(gs, 1);

    const auto& discard = gs.players[0].discard_pile;

    bool has_bulbasaur = std::any_of(discard.begin(), discard.end(),
        [&](const Card& c){ return c.id == bulbasaur.id; });
    bool has_ivysaur = std::any_of(discard.begin(), discard.end(),
        [&](const Card& c){ return c.id == ivysaur.id; });

    REQUIRE(has_bulbasaur);
    REQUIRE(has_ivysaur);

    std::cout << "  [PASS] test_ko_stage1_discards_full_chain\n";
}

// ---------------------------------------------------------------------------
// Test 8 (AC 7.8): KO of Stage 2 discards all three cards
// ---------------------------------------------------------------------------

static void test_ko_stage2_discards_full_chain()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur  = make_basic("Bulbasaur");
    Card ivysaur    = make_stage1("Ivysaur", "Bulbasaur");
    Card venusaur   = make_stage2("Venusaur", "Ivysaur");

    // Evolve Bulbasaur -> Ivysaur -> Venusaur
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);
    apply_evolve(gs, 0, ivysaur.id, 0);

    gs.players[0].hand.push_back(venusaur);
    apply_evolve(gs, 0, venusaur.id, 0);

    // Give opponent an active Pokemon so the game doesn't end immediately
    Card charmander = make_basic("Charmander", 10);
    gs.players[1].pokemon_slots[0] = make_in_play(charmander, false);
    // Give player 0 a bench Pokemon so they don't lose on KO
    Card squirtle = make_basic("Squirtle", 11);
    gs.players[0].pokemon_slots[1] = make_in_play(squirtle, false);

    // Verify chain before KO
    const InPlayPokemon& venusaur_ip = *gs.players[0].pokemon_slots[0];
    REQUIRE(venusaur_ip.card.id == venusaur.id);
    REQUIRE(venusaur_ip.cards_behind.size() == 2);

    // Deal lethal damage
    gs.players[0].pokemon_slots[0]->damage_counters = 999;

    resolve_knockouts(gs, 1);

    const auto& discard = gs.players[0].discard_pile;

    bool has_bulbasaur = std::any_of(discard.begin(), discard.end(),
        [&](const Card& c){ return c.id == bulbasaur.id; });
    bool has_ivysaur = std::any_of(discard.begin(), discard.end(),
        [&](const Card& c){ return c.id == ivysaur.id; });
    bool has_venusaur = std::any_of(discard.begin(), discard.end(),
        [&](const Card& c){ return c.id == venusaur.id; });

    REQUIRE(has_bulbasaur);
    REQUIRE(has_ivysaur);
    REQUIRE(has_venusaur);

    std::cout << "  [PASS] test_ko_stage2_discards_full_chain\n";
}

// ---------------------------------------------------------------------------
// Test 9: Evolve on bench slot works correctly
// ---------------------------------------------------------------------------

static void test_evolve_bench_slot()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 3;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    // Bulbasaur on bench slot 2
    gs.players[0].pokemon_slots[2] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(ivysaur);

    auto moves = generate_legal_moves(gs, 0);
    REQUIRE(has_evolve_for(moves, ivysaur.id, 2));

    // Apply the evolution
    apply_evolve(gs, 0, ivysaur.id, 2);

    const InPlayPokemon& evolved = *gs.players[0].pokemon_slots[2];
    REQUIRE(evolved.card.id == ivysaur.id);
    REQUIRE(evolved.cards_behind.size() == 1);
    REQUIRE(evolved.cards_behind[0].id == bulbasaur.id);

    std::cout << "  [PASS] test_evolve_bench_slot\n";
}

// ---------------------------------------------------------------------------
// Test 10: Evolution clears volatile status conditions
// ---------------------------------------------------------------------------

static void test_evolve_clears_volatile_status()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    Card ivysaur   = make_stage1("Ivysaur", "Bulbasaur");

    InPlayPokemon ip = make_in_play(bulbasaur, false);
    ip.status = StatusCondition::Paralyzed;
    gs.players[0].pokemon_slots[0] = ip;
    gs.players[0].hand.push_back(ivysaur);

    apply_evolve(gs, 0, ivysaur.id, 0);

    const InPlayPokemon& evolved = *gs.players[0].pokemon_slots[0];
    REQUIRE(evolved.status == StatusCondition::None);

    std::cout << "  [PASS] test_evolve_clears_volatile_status\n";
}

// ---------------------------------------------------------------------------
// Test 11: Wrong evolves_from does NOT generate Evolve action
// ---------------------------------------------------------------------------

static void test_no_evolve_wrong_evolves_from()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 3;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    // Charmeleon evolves from Charmander, not Bulbasaur
    Card charmeleon = make_stage1("Charmeleon", "Charmander");

    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);
    gs.players[0].hand.push_back(charmeleon);

    auto moves = generate_legal_moves(gs, 0);

    REQUIRE(!has_action_type(moves, ActionType::Evolve));

    std::cout << "  [PASS] test_no_evolve_wrong_evolves_from\n";
}

// ---------------------------------------------------------------------------
// Test 12: KO of basic (no evolution) only discards the single card
// ---------------------------------------------------------------------------

static void test_ko_basic_discards_only_itself()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.current_player = 0;

    Card bulbasaur = make_basic("Bulbasaur");
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur, false);

    // Give opponent an active Pokemon
    Card charmander = make_basic("Charmander", 10);
    gs.players[1].pokemon_slots[0] = make_in_play(charmander, false);
    // Give player 0 a bench Pokemon
    Card squirtle = make_basic("Squirtle", 11);
    gs.players[0].pokemon_slots[1] = make_in_play(squirtle, false);

    gs.players[0].pokemon_slots[0]->damage_counters = 999;

    resolve_knockouts(gs, 1);

    const auto& discard = gs.players[0].discard_pile;
    REQUIRE(discard.size() == 1);
    REQUIRE(discard[0].id == bulbasaur.id);

    std::cout << "  [PASS] test_ko_basic_discards_only_itself\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim evolution tests ===\n";

    RUN_TEST(test_evolve_action_generated_when_valid);
    RUN_TEST(test_evolve_blocked_when_played_this_turn);
    RUN_TEST(test_no_evolve_on_first_turn_player0);
    RUN_TEST(test_no_evolve_on_first_turn_player1);
    RUN_TEST(test_apply_evolve_removes_card_from_hand);
    RUN_TEST(test_apply_evolve_preserves_energy_tool_damage);
    RUN_TEST(test_apply_evolve_builds_cards_behind);
    RUN_TEST(test_ko_stage1_discards_full_chain);
    RUN_TEST(test_ko_stage2_discards_full_chain);
    RUN_TEST(test_evolve_bench_slot);
    RUN_TEST(test_evolve_clears_volatile_status);
    RUN_TEST(test_no_evolve_wrong_evolves_from);
    RUN_TEST(test_ko_basic_discards_only_itself);

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "All tests passed.\n";
    } else {
        std::cerr << g_failures << " test(s) FAILED.\n";
    }
    return g_failures > 0 ? 1 : 0;
}
