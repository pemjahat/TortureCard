// Unit tests for ExpectiMiniMaxPlayer and baseline_value_function.
//
// Build target: ptcgp_test_expectiminimax_player (registered in CMakeLists.txt)

#include "test_helpers.h"

#include "ptcgp_sim/expectiminimax_player.h"
#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/move_generation.h"
#include "ptcgp_sim/game_loop.h"
#include "ptcgp_sim/attach_attack_player.h"

#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// Test: baseline_value_function returns large positive for winner
// ---------------------------------------------------------------------------

static void test_baseline_value_fn_terminal_win()
{
    using namespace ptcgp_sim;

    Card dummy = make_pokemon("XX", 1, "Dummy", 60);
    GameState gs = make_game(dummy, dummy);
    gs.game_over = true;
    gs.winner    = 0;

    double score_p0 = baseline_value_function(gs, 0);
    double score_p1 = baseline_value_function(gs, 1);

    REQUIRE(score_p0 > 100'000.0);   // winner gets large positive
    REQUIRE(score_p1 < -100'000.0);  // loser gets large negative
}

// ---------------------------------------------------------------------------
// Test: baseline_value_function returns 0 for draw
// ---------------------------------------------------------------------------

static void test_baseline_value_fn_terminal_draw()
{
    using namespace ptcgp_sim;

    Card dummy = make_pokemon("XX", 1, "Dummy", 60);
    GameState gs = make_game(dummy, dummy);
    gs.game_over = true;
    gs.winner    = -1;

    double score_p0 = baseline_value_function(gs, 0);
    double score_p1 = baseline_value_function(gs, 1);

    REQUIRE(score_p0 == 0.0);
    REQUIRE(score_p1 == 0.0);
}

// ---------------------------------------------------------------------------
// Test: player with depth=1 selects the action with highest immediate value
// ---------------------------------------------------------------------------

static void test_depth1_selects_highest_immediate_value()
{
    using namespace ptcgp_sim;

    // Build a game where player 0 can attack and KO the opponent (win)
    // or just pass.  The player should prefer attacking.
    Attack atk = make_attack("Tackle", 60, {EnergyType::Fire});
    Card attacker = make_pokemon("XX", 1, "Attacker", 60, EnergyType::Fire, 0, {atk});
    Card defender = make_pokemon("XX", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);
    // Give attacker enough energy to attack
    gs.players[0].pokemon_slots[0]->attached_energy = {EnergyType::Fire};
    // Defender is at 50 damage — one 60-dmg hit KOs it
    gs.players[1].pokemon_slots[0]->damage_counters = 50;
    gs.current_energy = EnergyType::Fire; // energy available but already attached

    std::vector<Action> moves = generate_legal_moves(gs, 0);
    REQUIRE(!moves.empty());

    ExpectiMiniMaxPlayer player(1);
    Action chosen = player.decide(gs, moves);

    // Should choose Attack (because Opp Pokemon KO = win = highest value)
    REQUIRE(chosen.type == ActionType::Attack);
}

// ---------------------------------------------------------------------------
// Test: decide always returns one of the provided legal moves
// ---------------------------------------------------------------------------

static void test_decide_returns_legal_move()
{
    using namespace ptcgp_sim;

    Card dummy = make_pokemon("XX", 1, "Dummy", 60);
    GameState gs = make_game(dummy, dummy);
    gs.current_energy = EnergyType::Fire;

    std::vector<Action> moves = generate_legal_moves(gs, 0);
    REQUIRE(!moves.empty());

    ExpectiMiniMaxPlayer player(1);
    Action chosen = player.decide(gs, moves);

    bool found = false;
    for (const Action& m : moves)
    {
        if (m.type == chosen.type &&
            m.slot_index == chosen.slot_index &&
            m.target_slot == chosen.target_slot &&
            m.attack_index == chosen.attack_index &&
            m.card_id == chosen.card_id)
        {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// ---------------------------------------------------------------------------
// Test: full game completes without assertion failures
// ---------------------------------------------------------------------------

static void test_full_game_completes()
{
    using namespace ptcgp_sim;

    // Build minimal decks: 20 copies of a basic Pokemon
    Card basic = make_pokemon("XX", 1, "Basic", 60, EnergyType::Fire, 0,
                              {make_attack("Tackle", 30, {EnergyType::Fire})});
    Deck deck = make_uniform_deck(basic, EnergyType::Fire);

    std::mt19937 rng(42);
    GameState gs = GameState::make(deck, deck);
    gs.deal_starting_hands(rng);

    ExpectiMiniMaxPlayer emm(1);   // depth=1 for speed
    AttachAttackPlayer   aap;

    GameLoop loop(&emm, &aap, rng, false);
    SimulationResult result = loop.run(gs);

    // Game must end with a valid winner or draw
    REQUIRE(result.winner == 0 || result.winner == 1 || result.winner == -1);
    REQUIRE(result.turns > 0);
}

// ---------------------------------------------------------------------------
// Test: depth=1 behaves like a value-function player
// ---------------------------------------------------------------------------

static void test_depth1_equals_value_function_player()
{
    using namespace ptcgp_sim;

    // Build a state with multiple legal moves and verify depth=1 picks the
    // same action as directly evaluating the value function on each outcome.
    Attack atk = make_attack("Tackle", 30, {EnergyType::Fire});
    Card pokemon = make_pokemon("XX", 1, "Pokemon", 60, EnergyType::Fire, 0, {atk});
    GameState gs = make_game(pokemon, pokemon);
    gs.players[0].pokemon_slots[0]->attached_energy = {EnergyType::Fire};
    gs.current_energy = EnergyType::Fire;

    std::vector<Action> moves = generate_legal_moves(gs, 0);
    REQUIRE(!moves.empty());

    ExpectiMiniMaxPlayer player(1);
    Action chosen = player.decide(gs, moves);

    // Manually find the best action by applying each and evaluating
    std::mt19937 rng(42);
    double best_score = -std::numeric_limits<double>::infinity();
    Action best_action = moves[0];
    for (const Action& m : moves)
    {
        GameState copy = gs;
        apply_action(copy, m, rng);
        double score = baseline_value_function(copy, 0);
        if (score > best_score)
        {
            best_score  = score;
            best_action = m;
        }
    }

    REQUIRE(chosen.type == best_action.type);
}

// ---------------------------------------------------------------------------
// Test: value function is called for the opponent's player index at depth >= 2
//
// This verifies that after a turn-ending action (Attack or Pass), the search
// actually transitions to the opponent's turn.  We inject a custom value
// function that records every (state.current_player) it is called with.
// At depth=2 the search must evaluate at least one state where
// current_player == 1 (the opponent), proving the MIN node is reached.
// ---------------------------------------------------------------------------

static void test_opponent_turn_reached_at_depth2()
{
    using namespace ptcgp_sim;

    // Simple game: both sides have a basic Pokemon, no attacks needed.
    Card dummy = make_pokemon("XX", 1, "Dummy", 60);
    GameState gs = make_game(dummy, dummy);
    gs.current_energy = EnergyType::Fire;

    std::vector<Action> moves = generate_legal_moves(gs, 0);
    REQUIRE(!moves.empty());

    // Custom value function: records every current_player it sees.
    std::vector<int> seen_players;
    ValueFunction tracking_vf = [&](const GameState& s, int /*player*/) -> double
    {
        seen_players.push_back(s.current_player);
        return baseline_value_function(s, 0);
    };

    ExpectiMiniMaxPlayer player(tracking_vf, /*depth=*/2);
    player.decide(gs, moves);

    // At depth=2 the search must have evaluated at least one state where
    // current_player == 1 (opponent's turn after our turn-ending action).
    bool saw_opponent = false;
    for (int p : seen_players)
        if (p == 1) { saw_opponent = true; break; }

    REQUIRE(saw_opponent);
}

// ---------------------------------------------------------------------------
// Test: depth-2 score differs from depth-1 when opponent can KO us
//
// Setup:
//   - Our active: 60 HP, 30-dmg attack (1 Fire energy cost), has Fire energy
//   - Opponent active: 60 HP, 40-dmg attack (1 Fire energy cost), has Fire energy
//   - Our active has 20 damage counters (40 HP remaining)
//
// At depth=1:
//   Our Attack → opponent takes 30 dmg (not KO'd) → VF called (non-terminal)
//   Score is a heuristic (not ±1,000,000)
//
// At depth=2:
//   Our Attack → opponent takes 30 dmg → opponent's turn →
//   Opponent attacks us for 40 dmg → our 40 HP active KO'd → game_over, winner=1
//   VF returns -1,000,000
//
// Therefore score_d1 != score_d2, and score_d2 == -1,000,000.
// ---------------------------------------------------------------------------

static void test_depth2_score_differs_from_depth1()
{
    using namespace ptcgp_sim;

    Attack our_atk = make_attack("Scratch", 30, {EnergyType::Fire});
    Card our_card  = make_pokemon("XX", 1, "OurMon", 60, EnergyType::Fire, 0, {our_atk});

    Attack opp_atk = make_attack("Slash", 40, {EnergyType::Fire});
    Card opp_card  = make_pokemon("XX", 2, "OppMon", 60, EnergyType::Fire, 0, {opp_atk});

    // Our active has 20 damage counters => 40 HP remaining
    // Opponent's 40-dmg attack will KO us next turn
    GameState gs = make_game(our_card, opp_card, /*p0_dmg=*/20, /*p1_dmg=*/0);
    gs.players[0].pokemon_slots[0]->attached_energy = {EnergyType::Fire};
    gs.players[1].pokemon_slots[0]->attached_energy = {EnergyType::Fire};
    gs.current_energy = EnergyType::Fire;

    // Find the Attack action
    std::vector<Action> all_moves = generate_legal_moves(gs, 0);
    Action attack_action = all_moves[0];
    bool found = false;
    for (const Action& m : all_moves)
        if (m.type == ActionType::Attack) { attack_action = m; found = true; break; }
    REQUIRE(found);

    // Depth=1: evaluate only our attack outcome (opponent not yet acting)
    double score_d1 = 0.0;
    ValueFunction capture_d1 = [&](const GameState& s, int player) -> double
    {
        score_d1 = baseline_value_function(s, player);
        return score_d1;
    };
    ExpectiMiniMaxPlayer player_d1(capture_d1, 1);
    player_d1.decide(gs, {attack_action});

    // Depth=2: opponent gets to act and KOs us
    double score_d2 = 0.0;
    ValueFunction capture_d2 = [&](const GameState& s, int player) -> double
    {
        score_d2 = baseline_value_function(s, player);
        return score_d2;
    };
    ExpectiMiniMaxPlayer player_d2(capture_d2, 2);
    player_d2.decide(gs, {attack_action});

    // depth=1 sees a non-terminal state (heuristic score, not ±1,000,000)
    REQUIRE(std::abs(score_d1) < 1'000'000.0);

    // depth=2 sees the opponent KO us: game_over, winner=1 => score = -1,000,000
    REQUIRE(score_d2 == -1'000'000.0);

    // The two scores must differ
    REQUIRE(score_d1 != score_d2);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim expectiminimax player tests ===\n";

    RUN_TEST(test_baseline_value_fn_terminal_win);
    RUN_TEST(test_baseline_value_fn_terminal_draw);
    RUN_TEST(test_depth1_selects_highest_immediate_value);
    RUN_TEST(test_decide_returns_legal_move);
    RUN_TEST(test_full_game_completes);
    RUN_TEST(test_depth1_equals_value_function_player);
    RUN_TEST(test_opponent_turn_reached_at_depth2);
    RUN_TEST(test_depth2_score_differs_from_depth1);

    return ptcgp_test::print_summary();
}
