// Unit tests for apply_action (effect resolution):
//   - Normal attack damage (Requirement 1)
//   - Knockout and discard resolution (Requirement 2)
//   - Basic Item: Pokemon search / Poke Ball (Requirement 3)
//
// Uses the same try/catch test infrastructure as the other test files.
// Build target: ptcgp_test_effects (added in CMakeLists.txt)

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
    const std::vector<ptcgp_sim::Attack>& attacks = {},
    std::optional<ptcgp_sim::EnergyType> weakness = std::nullopt)
{
    ptcgp_sim::Card c;
    c.id          = {expansion, number};
    c.name        = name;
    c.type        = ptcgp_sim::CardType::Pokemon;
    c.hp          = hp;
    c.energy_type = energy_type;
    c.stage       = stage;
    c.attacks     = attacks;
    c.weakness    = weakness;
    return c;
}

static ptcgp_sim::Card make_trainer(
    const std::string& expansion, int number,
    const std::string& name,
    ptcgp_sim::TrainerType tt = ptcgp_sim::TrainerType::Item)
{
    ptcgp_sim::Card c;
    c.id           = {expansion, number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Trainer;
    c.trainer_type = tt;
    return c;
}

static ptcgp_sim::Attack make_attack(const std::string& name, int damage,
                                      std::vector<ptcgp_sim::EnergyType> cost = {})
{
    ptcgp_sim::Attack a;
    a.name            = name;
    a.damage          = damage;
    a.energy_required = std::move(cost);
    return a;
}

// Build a minimal 20-card deck (all copies of the same card)
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

// Build a fresh GameState with two minimal decks and both actives placed.
static ptcgp_sim::GameState make_game(
    const ptcgp_sim::Card& p0_active,
    const ptcgp_sim::Card& p1_active)
{
    using namespace ptcgp_sim;

    Card dummy = make_pokemon("XX", 99, "Dummy", 60);
    Deck deck  = make_deck(std::vector<Card>(20, dummy));

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
// REQUIREMENT 1: Normal Attack Damage Resolution
// ============================================================================

// ---------------------------------------------------------------------------
// Test R1-1: Base damage is applied to defender's damage_counters
// ---------------------------------------------------------------------------
static void test_attack_applies_base_damage()
{
    using namespace ptcgp_sim;

    Attack ember = make_attack("Ember", 30, {EnergyType::Fire});
    Card charmander = make_pokemon("A1", 33, "Charmander", 60,
                                   EnergyType::Fire, 0, {ember});
    Card squirtle   = make_pokemon("A1", 60, "Squirtle", 60);

    GameState gs = make_game(charmander, squirtle);
    gs.players[0].pokemon_slots[0]->attached_energy.push_back(EnergyType::Fire);

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 30);
    REQUIRE(gs.players[1].pokemon_slots[0]->remaining_hp() == 30);
    REQUIRE(gs.attacked_this_turn == true);

    std::cout << "  [PASS] test_attack_applies_base_damage\n";
}

// ---------------------------------------------------------------------------
// Test R1-2: Weakness adds +20 damage
// ---------------------------------------------------------------------------
static void test_attack_weakness_adds_20()
{
    using namespace ptcgp_sim;

    Attack ember = make_attack("Ember", 30, {EnergyType::Fire});
    Card charmander = make_pokemon("A1", 33, "Charmander", 60,
                                   EnergyType::Fire, 0, {ember});
    // Squirtle has weakness to Fire
    Card squirtle = make_pokemon("A1", 60, "Squirtle", 60,
                                  EnergyType::Water, 0, {},
                                  EnergyType::Fire /*weakness*/);

    GameState gs = make_game(charmander, squirtle);
    gs.players[0].pokemon_slots[0]->attached_energy.push_back(EnergyType::Fire);

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // 30 base + 20 weakness = 50
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 50);
    REQUIRE(gs.players[1].pokemon_slots[0]->remaining_hp() == 10);

    std::cout << "  [PASS] test_attack_weakness_adds_20\n";
}

// ---------------------------------------------------------------------------
// Test R1-3: No weakness bonus when types don't match
// ---------------------------------------------------------------------------
static void test_attack_no_weakness_when_type_mismatch()
{
    using namespace ptcgp_sim;

    Attack tackle = make_attack("Tackle", 20);
    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 60,
                                   EnergyType::Grass, 0, {tackle});
    // Squirtle has weakness to Lightning, not Grass
    Card squirtle = make_pokemon("A1", 60, "Squirtle", 60,
                                  EnergyType::Water, 0, {},
                                  EnergyType::Lightning /*weakness*/);

    GameState gs = make_game(bulbasaur, squirtle);

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 20);

    std::cout << "  [PASS] test_attack_no_weakness_when_type_mismatch\n";
}

// ---------------------------------------------------------------------------
// Test R1-4: Zero-damage attack does not change damage_counters
// ---------------------------------------------------------------------------
static void test_attack_zero_damage_no_change()
{
    using namespace ptcgp_sim;

    Attack growl = make_attack("Growl", 0);
    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 60,
                                   EnergyType::Grass, 0, {growl});
    Card squirtle  = make_pokemon("A1", 60, "Squirtle", 60);

    GameState gs = make_game(bulbasaur, squirtle);

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 0);
    REQUIRE(gs.attacked_this_turn == true);

    std::cout << "  [PASS] test_attack_zero_damage_no_change\n";
}

// ---------------------------------------------------------------------------
// Test R1-5: damage_counters are capped at card.hp
// ---------------------------------------------------------------------------
static void test_attack_damage_capped_at_hp()
{
    using namespace ptcgp_sim;

    Attack megapunch = make_attack("Mega Punch", 200);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {megapunch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);

    // Verify cap by calling apply_attack_damage directly (does not resolve KO)
    int damage_applied = apply_attack_damage(gs, 0, 0);

    // damage_counters must not exceed hp (60), even though attack does 200
    REQUIRE(gs.players[1].pokemon_slots[0].has_value());
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 60);
    REQUIRE(gs.players[1].pokemon_slots[0]->remaining_hp() == 0);
    REQUIRE(gs.players[1].pokemon_slots[0]->is_knocked_out() == true);
    // The returned damage value is the raw (pre-cap) damage
    REQUIRE(damage_applied == 200);

    std::cout << "  [PASS] test_attack_damage_capped_at_hp\n";
}

// ============================================================================
// REQUIREMENT 2: Knockout and Discard Resolution
// ============================================================================

// ---------------------------------------------------------------------------
// Test R2-1: KO'd Pokemon is moved to discard pile
// ---------------------------------------------------------------------------
static void test_ko_pokemon_moved_to_discard()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    // Give defender a bench so game doesn't end
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Active slot should be empty
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value());
    // Defender card should be in discard pile
    REQUIRE(gs.players[1].discard_pile.size() == 1);
    REQUIRE(gs.players[1].discard_pile[0].name == "Defender");

    std::cout << "  [PASS] test_ko_pokemon_moved_to_discard\n";
}

// ---------------------------------------------------------------------------
// Test R2-2: KO awards 1 prize point to the opponent
// ---------------------------------------------------------------------------
static void test_ko_awards_1_point()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    // Give defender a bench so game doesn't end
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.players[0].points == 1);
    REQUIRE(gs.players[1].points == 0);

    std::cout << "  [PASS] test_ko_awards_1_point\n";
}

// ---------------------------------------------------------------------------
// Test R2-3: 3 points triggers game_over and sets winner
// ---------------------------------------------------------------------------
static void test_3_points_triggers_game_over()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    gs.players[0].points = 2; // one more KO wins
    // Give defender a bench so the "no bench" path isn't triggered
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.game_over == true);
    REQUIRE(gs.winner == 0);

    std::cout << "  [PASS] test_3_points_triggers_game_over\n";
}

// ---------------------------------------------------------------------------
// Test R2-4: KO with no bench -> opponent wins immediately
// ---------------------------------------------------------------------------
static void test_ko_no_bench_opponent_wins()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    // No bench for player 1

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.game_over == true);
    REQUIRE(gs.winner == 0); // player 0 wins
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value());

    std::cout << "  [PASS] test_ko_no_bench_opponent_wins\n";
}

// ---------------------------------------------------------------------------
// Test R2-5: KO with bench -> active slot empty, game continues
// ---------------------------------------------------------------------------
static void test_ko_with_bench_game_continues()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    REQUIRE(gs.game_over == false);
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value()); // active is empty
    REQUIRE(gs.players[1].pokemon_slots[1].has_value());  // bench still there

    std::cout << "  [PASS] test_ko_with_bench_game_continues\n";
}

// ---------------------------------------------------------------------------
// Test R2-6: Bench KO discards card and awards point, no promotion logic
// ---------------------------------------------------------------------------
static void test_bench_ko_discards_and_awards_point()
{
    using namespace ptcgp_sim;

    // Use apply_attack_damage + resolve_knockouts directly to simulate
    // bench damage (attack normally only hits active, but we can pre-set counters)
    Card attacker_card = make_pokemon("A1", 1, "Attacker", 60);
    Card bench_card    = make_pokemon("A1", 5, "BenchVictim", 60);

    Card dummy_p0 = make_pokemon("XX", 99, "Dummy", 60);
    Card dummy_p1 = make_pokemon("XX", 98, "Dummy2", 60);
    Deck deck = make_deck(std::vector<ptcgp_sim::Card>(20, dummy_p0));

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;

    InPlayPokemon ip0; ip0.card = attacker_card;
    InPlayPokemon ip1; ip1.card = dummy_p1;
    InPlayPokemon bench_ip; bench_ip.card = bench_card;
    bench_ip.damage_counters = 60; // pre-KO'd

    gs.players[0].pokemon_slots[0] = ip0;
    gs.players[1].pokemon_slots[0] = ip1;
    gs.players[1].pokemon_slots[1] = bench_ip;

    resolve_knockouts(gs, 0);

    // Bench slot 1 should be cleared
    REQUIRE(!gs.players[1].pokemon_slots[1].has_value());
    // Active slot 0 of player 1 should still be there
    REQUIRE(gs.players[1].pokemon_slots[0].has_value());
    // Point awarded to player 0
    REQUIRE(gs.players[0].points == 1);
    // Game not over (active still alive)
    REQUIRE(gs.game_over == false);

    std::cout << "  [PASS] test_bench_ko_discards_and_awards_point\n";
}

// ---------------------------------------------------------------------------
// Test R2-7: Attached tool is also discarded on KO
// ---------------------------------------------------------------------------
static void test_ko_discards_attached_tool()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 60,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card tool     = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = tool;
    // Give bench so game doesn't end
    Card bench_mon = make_pokemon("A1", 3, "BenchMon", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench_mon;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Both the Pokemon and the tool should be in discard
    REQUIRE(gs.players[1].discard_pile.size() == 2);
    bool has_defender = false, has_tool = false;
    for (const auto& c : gs.players[1].discard_pile)
    {
        if (c.name == "Defender")    has_defender = true;
        if (c.name == "Rocky Helmet") has_tool    = true;
    }
    REQUIRE(has_defender);
    REQUIRE(has_tool);

    std::cout << "  [PASS] test_ko_discards_attached_tool\n";
}

// ============================================================================
// REQUIREMENT 3: Basic Item — Pokemon Search (Poke Ball)
// ============================================================================

// ---------------------------------------------------------------------------
// Test R3-1: Poke Ball finds a Basic Pokemon and puts it in hand
// ---------------------------------------------------------------------------
static void test_pokeball_finds_basic_pokemon()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 60,
                                   EnergyType::Grass, 0 /*stage*/);
    Card poke_ball = make_trainer("PA", 5, "Poke Ball", TrainerType::Item);

    // Deck: 19 Bulbasaurs + 1 dummy trainer (to reach 20 cards)
    Card dummy_trainer = make_trainer("PA", 99, "Dummy Item");
    std::vector<Card> deck_cards(19, bulbasaur);
    deck_cards.push_back(dummy_trainer);
    Deck deck = make_deck(deck_cards);

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;

    // Put Poke Ball in hand
    gs.players[0].hand.push_back(poke_ball);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(poke_ball.id), rng);

    // Hand should now contain one Bulbasaur (Poke Ball was discarded)
    bool has_bulbasaur = std::any_of(gs.players[0].hand.begin(),
                                      gs.players[0].hand.end(),
                                      [](const Card& c){ return c.name == "Bulbasaur"; });
    REQUIRE(has_bulbasaur);

    // Poke Ball should be in discard pile
    bool poke_ball_discarded = std::any_of(gs.players[0].discard_pile.begin(),
                                            gs.players[0].discard_pile.end(),
                                            [](const Card& c){ return c.name == "Poke Ball"; });
    REQUIRE(poke_ball_discarded);

    std::cout << "  [PASS] test_pokeball_finds_basic_pokemon\n";
}

// ---------------------------------------------------------------------------
// Test R3-2: Poke Ball with no Basic in deck — hand unchanged, deck shuffled
// ---------------------------------------------------------------------------
static void test_pokeball_no_basic_in_deck()
{
    using namespace ptcgp_sim;

    Card poke_ball    = make_trainer("PA", 5, "Poke Ball", TrainerType::Item);
    Card dummy_item   = make_trainer("PA", 99, "Dummy Item");
    Card stage1_mon   = make_pokemon("A1", 10, "Ivysaur", 90,
                                      EnergyType::Grass, 1 /*stage 1*/);

    // Deck: 10 Ivysaurs (stage 1) + 10 dummy items — no Basic Pokemon
    std::vector<Card> deck_cards(10, stage1_mon);
    for (int i = 0; i < 10; ++i) deck_cards.push_back(dummy_item);
    Deck deck = make_deck(deck_cards);

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    gs.players[0].hand.push_back(poke_ball);

    std::size_t hand_size_before = gs.players[0].hand.size();
    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(poke_ball.id), rng);

    // Hand size should be hand_size_before - 1 (Poke Ball removed, nothing added)
    REQUIRE(gs.players[0].hand.size() == hand_size_before - 1);

    // Poke Ball should be in discard
    bool poke_ball_discarded = std::any_of(gs.players[0].discard_pile.begin(),
                                            gs.players[0].discard_pile.end(),
                                            [](const Card& c){ return c.name == "Poke Ball"; });
    REQUIRE(poke_ball_discarded);

    std::cout << "  [PASS] test_pokeball_no_basic_in_deck\n";
}

// ---------------------------------------------------------------------------
// Test R3-3: Poke Ball removes the found card from the deck
// ---------------------------------------------------------------------------
static void test_pokeball_removes_card_from_deck()
{
    using namespace ptcgp_sim;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 60,
                                   EnergyType::Grass, 0);
    Card poke_ball = make_trainer("PA", 5, "Poke Ball", TrainerType::Item);

    std::vector<Card> deck_cards(20, bulbasaur);
    Deck deck = make_deck(deck_cards);

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    gs.players[0].hand.push_back(poke_ball);

    std::size_t deck_size_before = gs.players[0].deck.cards.size();
    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(poke_ball.id), rng);

    // Deck should have one fewer card
    REQUIRE(gs.players[0].deck.cards.size() == deck_size_before - 1);

    std::cout << "  [PASS] test_pokeball_removes_card_from_deck\n";
}

// ---------------------------------------------------------------------------
// Test R3-4: Unknown item is discarded with no crash (no-op effect)
// ---------------------------------------------------------------------------
static void test_unknown_item_discarded_no_crash()
{
    using namespace ptcgp_sim;

    Card unknown_item = make_trainer("ZZ", 999, "Mystery Item", TrainerType::Item);

    Card dummy_mon = make_pokemon("A1", 1, "Dummy", 60);
    Deck deck = make_deck(std::vector<Card>(20, dummy_mon));

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    gs.players[0].hand.push_back(unknown_item);

    std::size_t hand_size_before = gs.players[0].hand.size();
    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(unknown_item.id), rng);

    // Item removed from hand
    REQUIRE(gs.players[0].hand.size() == hand_size_before - 1);
    // Item in discard
    REQUIRE(gs.players[0].discard_pile.size() == 1);
    REQUIRE(gs.players[0].discard_pile[0].name == "Mystery Item");
    // Deck unchanged
    REQUIRE(gs.players[0].deck.cards.size() == 20);

    std::cout << "  [PASS] test_unknown_item_discarded_no_crash\n";
}

// ---------------------------------------------------------------------------
// Test R3-5: PlayItem does NOT set supporter_played_this_turn
// ---------------------------------------------------------------------------
static void test_item_does_not_set_supporter_flag()
{
    using namespace ptcgp_sim;

    Card poke_ball = make_trainer("PA", 5, "Poke Ball", TrainerType::Item);
    Card dummy_mon = make_pokemon("A1", 1, "Dummy", 60);
    Deck deck = make_deck(std::vector<Card>(20, dummy_mon));

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    gs.players[0].hand.push_back(poke_ball);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(poke_ball.id), rng);

    REQUIRE(gs.supporter_played_this_turn == false);

    std::cout << "  [PASS] test_item_does_not_set_supporter_flag\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== ptcgp_sim effect resolution tests ===\n";

    // Requirement 1: Attack Damage
    RUN_TEST(test_attack_applies_base_damage);
    RUN_TEST(test_attack_weakness_adds_20);
    RUN_TEST(test_attack_no_weakness_when_type_mismatch);
    RUN_TEST(test_attack_zero_damage_no_change);
    RUN_TEST(test_attack_damage_capped_at_hp);

    // Requirement 2: Knockout & Discard
    RUN_TEST(test_ko_pokemon_moved_to_discard);
    RUN_TEST(test_ko_awards_1_point);
    RUN_TEST(test_3_points_triggers_game_over);
    RUN_TEST(test_ko_no_bench_opponent_wins);
    RUN_TEST(test_ko_with_bench_game_continues);
    RUN_TEST(test_bench_ko_discards_and_awards_point);
    RUN_TEST(test_ko_discards_attached_tool);

    // Requirement 3: Poke Ball / Basic Search Item
    RUN_TEST(test_pokeball_finds_basic_pokemon);
    RUN_TEST(test_pokeball_no_basic_in_deck);
    RUN_TEST(test_pokeball_removes_card_from_deck);
    RUN_TEST(test_unknown_item_discarded_no_crash);
    RUN_TEST(test_item_does_not_set_supporter_flag);

    std::cout << "\n";
    if (g_failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cerr << g_failures << " test(s) FAILED.\n";

    return g_failures > 0 ? 1 : 0;
}
