// Unit tests for generate_legal_moves and energy_satisfies_cost.
// Uses assert-based testing — no external framework required.
// Build target: ptcgp_test (added in CMakeLists.txt)

#include "ptcgp_sim/action.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/move_generation.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ptcgp_sim::Card make_pokemon(const std::string& expansion, int number,
                                    const std::string& name, int stage = 0,
                                    const std::vector<ptcgp_sim::Attack>& attacks = {})
{
    ptcgp_sim::Card c;
    c.id        = {expansion, number};
    c.name      = name;
    c.type      = ptcgp_sim::CardType::Pokemon;
    c.hp        = 60;
    c.stage     = stage;
    c.attacks   = attacks;
    return c;
}

static ptcgp_sim::Card make_trainer(const std::string& expansion, int number,
                                    const std::string& name,
                                    ptcgp_sim::TrainerType tt)
{
    ptcgp_sim::Card c;
    c.id           = {expansion, number};
    c.name         = name;
    c.type         = ptcgp_sim::CardType::Trainer;
    c.trainer_type = tt;
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

static bool has_action_type(const std::vector<ptcgp_sim::Action>& moves,
                             ptcgp_sim::ActionType t)
{
    return std::any_of(moves.begin(), moves.end(),
                       [t](const ptcgp_sim::Action& a){ return a.type == t; });
}

static int count_action_type(const std::vector<ptcgp_sim::Action>& moves,
                              ptcgp_sim::ActionType t)
{
    return static_cast<int>(
        std::count_if(moves.begin(), moves.end(),
                      [t](const ptcgp_sim::Action& a){ return a.type == t; }));
}

// ---------------------------------------------------------------------------
// Test: energy_satisfies_cost
// ---------------------------------------------------------------------------

static void test_energy_satisfies_cost()
{
    using namespace ptcgp_sim;

    // Free attack (no cost)
    assert(energy_satisfies_cost({}, {}));
    assert(energy_satisfies_cost({EnergyType::Fire}, {}));

    // Exact typed match
    assert(energy_satisfies_cost({EnergyType::Fire}, {EnergyType::Fire}));

    // Wrong type
    assert(!energy_satisfies_cost({EnergyType::Water}, {EnergyType::Fire}));

    // Colorless matches any type
    assert(energy_satisfies_cost({EnergyType::Fire}, {EnergyType::Colorless}));
    assert(energy_satisfies_cost({EnergyType::Water}, {EnergyType::Colorless}));

    // Mixed: typed + colorless
    assert(energy_satisfies_cost({EnergyType::Fire, EnergyType::Water},
                                  {EnergyType::Fire, EnergyType::Colorless}));

    // Not enough energy
    assert(!energy_satisfies_cost({EnergyType::Fire},
                                   {EnergyType::Fire, EnergyType::Colorless}));

    std::cout << "  [PASS] test_energy_satisfies_cost\n";
}

// ---------------------------------------------------------------------------
// Test 1: Setup phase — Basic Pokemon in hand -> PlayPokemon to slot 0 only
// ---------------------------------------------------------------------------

static void test_setup_basic_pokemon_to_active()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Setup;
    gs.current_player = 0;

    Card bulbasaur = make_pokemon("A1", 1, "Bulbasaur", 0);
    gs.players[0].hand.push_back(bulbasaur);

    auto moves = generate_legal_moves(gs, 0);

    // Should have exactly one PlayPokemon targeting slot 0
    assert(count_action_type(moves, ActionType::PlayPokemon) == 1);
    assert(moves[0].type == ActionType::PlayPokemon);
    assert(moves[0].slot_index == 0);
    // No Pass yet (active not filled)
    assert(!has_action_type(moves, ActionType::Pass));

    std::cout << "  [PASS] test_setup_basic_pokemon_to_active\n";
}

// ---------------------------------------------------------------------------
// Test 2: Setup phase — active filled -> bench slots + Pass
// ---------------------------------------------------------------------------

static void test_setup_active_filled_bench_and_pass()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Setup;
    gs.current_player = 0;

    Card bulbasaur  = make_pokemon("A1", 1, "Bulbasaur", 0);
    Card charmander = make_pokemon("A1", 33, "Charmander", 0);

    // Active already occupied
    gs.players[0].pokemon_slots[0] = make_in_play(bulbasaur);
    // Charmander in hand
    gs.players[0].hand.push_back(charmander);

    auto moves = generate_legal_moves(gs, 0);

    // Should offer bench slots 1-3 for Charmander + Pass
    assert(count_action_type(moves, ActionType::PlayPokemon) == 3); // slots 1,2,3
    assert(has_action_type(moves, ActionType::Pass));

    std::cout << "  [PASS] test_setup_active_filled_bench_and_pass\n";
}

// ---------------------------------------------------------------------------
// Test 3: Action phase — AttachEnergy generated when energy available
// ---------------------------------------------------------------------------

static void test_action_attach_energy_generated()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase              = TurnPhase::Action;
    gs.current_player          = 0;
    gs.current_energy          = EnergyType::Fire;
    gs.energy_attached_this_turn = false;

    Card charmander = make_pokemon("A1", 33, "Charmander", 0);
    gs.players[0].pokemon_slots[0] = make_in_play(charmander);

    auto moves = generate_legal_moves(gs, 0);

    assert(has_action_type(moves, ActionType::AttachEnergy));

    std::cout << "  [PASS] test_action_attach_energy_generated\n";
}

// ---------------------------------------------------------------------------
// Test 4: Action phase — Attack generated when energy requirement met
// ---------------------------------------------------------------------------

static void test_action_attack_generated_when_energy_met()
{
    using namespace ptcgp_sim;

    Attack ember;
    ember.name             = "Ember";
    ember.energy_required  = {EnergyType::Fire};
    ember.damage           = 30;

    Card charmander = make_pokemon("A1", 33, "Charmander", 0, {ember});

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;

    InPlayPokemon ip = make_in_play(charmander);
    ip.attached_energy.push_back(EnergyType::Fire);
    gs.players[0].pokemon_slots[0] = ip;

    auto moves = generate_legal_moves(gs, 0);

    assert(has_action_type(moves, ActionType::Attack));

    std::cout << "  [PASS] test_action_attack_generated_when_energy_met\n";
}

// ---------------------------------------------------------------------------
// Test 5: Action phase — Attack NOT generated when energy insufficient
// ---------------------------------------------------------------------------

static void test_action_attack_not_generated_when_energy_insufficient()
{
    using namespace ptcgp_sim;

    Attack ember;
    ember.name             = "Ember";
    ember.energy_required  = {EnergyType::Fire, EnergyType::Fire};
    ember.damage           = 50;

    Card charmander = make_pokemon("A1", 33, "Charmander", 0, {ember});

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;

    InPlayPokemon ip = make_in_play(charmander);
    ip.attached_energy.push_back(EnergyType::Fire); // only 1, need 2
    gs.players[0].pokemon_slots[0] = ip;

    auto moves = generate_legal_moves(gs, 0);

    assert(!has_action_type(moves, ActionType::Attack));

    std::cout << "  [PASS] test_action_attack_not_generated_when_energy_insufficient\n";
}

// ---------------------------------------------------------------------------
// Test 6: Pass is always present in Action phase
// ---------------------------------------------------------------------------

static void test_pass_always_present_in_action_phase()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    // Empty hand, no pokemon in play

    auto moves = generate_legal_moves(gs, 0);

    assert(has_action_type(moves, ActionType::Pass));

    std::cout << "  [PASS] test_pass_always_present_in_action_phase\n";
}

// ---------------------------------------------------------------------------
// Test 7: Turn 1 — no AttachEnergy (energy generation starts turn 2)
// ---------------------------------------------------------------------------

static void test_no_attach_energy_on_turn_1()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 1;
    gs.current_player = 0;
    gs.current_energy = std::nullopt; // no energy on turn 1

    Card charmander = make_pokemon("A1", 33, "Charmander", 0);
    gs.players[0].pokemon_slots[0] = make_in_play(charmander);

    auto moves = generate_legal_moves(gs, 0);

    assert(!has_action_type(moves, ActionType::AttachEnergy));

    std::cout << "  [PASS] test_no_attach_energy_on_turn_1\n";
}

// ---------------------------------------------------------------------------
// Test 8: PlayTool NOT generated for slot that already has a Tool
// ---------------------------------------------------------------------------

static void test_play_tool_blocked_when_slot_has_tool()
{
    using namespace ptcgp_sim;

    Card rocky_helmet = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs;
    gs.turn_phase     = TurnPhase::Action;
    gs.current_player = 0;
    gs.players[0].hand.push_back(rocky_helmet);

    Card charmander = make_pokemon("A1", 33, "Charmander", 0);
    InPlayPokemon ip = make_in_play(charmander);
    // Attach a tool already
    ip.attached_tool = rocky_helmet;
    gs.players[0].pokemon_slots[0] = ip;

    auto moves = generate_legal_moves(gs, 0);

    // No PlayTool should target slot 0 (already has tool)
    bool has_tool_to_slot0 = std::any_of(moves.begin(), moves.end(),
        [](const Action& a){ return a.type == ActionType::PlayTool && a.slot_index == 0; });
    assert(!has_tool_to_slot0);

    std::cout << "  [PASS] test_play_tool_blocked_when_slot_has_tool\n";
}

// ---------------------------------------------------------------------------
// Test 9: PlaySupporter NOT generated after one already played this turn
// ---------------------------------------------------------------------------

static void test_play_supporter_blocked_after_one_played()
{
    using namespace ptcgp_sim;

    Card misty = make_trainer("A1", 200, "Misty", TrainerType::Supporter);

    GameState gs;
    gs.turn_phase                = TurnPhase::Action;
    gs.current_player            = 0;
    gs.supporter_played_this_turn = true; // already played one
    gs.players[0].hand.push_back(misty);

    auto moves = generate_legal_moves(gs, 0);

    assert(!has_action_type(moves, ActionType::PlaySupporter));

    std::cout << "  [PASS] test_play_supporter_blocked_after_one_played\n";
}

// ---------------------------------------------------------------------------
// Test 10: PlayStadium generated even when a Stadium is already in play
// ---------------------------------------------------------------------------

static void test_play_stadium_always_legal()
{
    using namespace ptcgp_sim;

    Card stadium = make_trainer("A1", 250, "Poke Center", TrainerType::Stadium);

    GameState gs;
    gs.turn_phase       = TurnPhase::Action;
    gs.current_player   = 0;
    gs.current_stadium  = ptcgp_sim::CardId{"A1", 249}; // some other stadium already active
    gs.players[0].hand.push_back(stadium);

    auto moves = generate_legal_moves(gs, 0);

    assert(has_action_type(moves, ActionType::PlayStadium));

    std::cout << "  [PASS] test_play_stadium_always_legal\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim move generation tests ===\n";

    test_energy_satisfies_cost();
    test_setup_basic_pokemon_to_active();
    test_setup_active_filled_bench_and_pass();
    test_action_attach_energy_generated();
    test_action_attack_generated_when_energy_met();
    test_action_attack_not_generated_when_energy_insufficient();
    test_pass_always_present_in_action_phase();
    test_no_attach_energy_on_turn_1();
    test_play_tool_blocked_when_slot_has_tool();
    test_play_supporter_blocked_after_one_played();
    test_play_stadium_always_legal();

    std::cout << "\nAll tests passed.\n";
    return 0;
}
