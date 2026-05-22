// Unit tests for Sabrina and Cyrus supporter card effects.
// Covers: priority-passing state, move generation legality, and effect resolution.
// Build target: ptcgp_test_supporter_effects (added in CMakeLists.txt)

#include "test_helpers.h"

#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/move_generation.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

// ============================================================================
// Sabrina move-generation tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test S1: Sabrina in hand, opponent has 1 bench -> 1 PlaySupporter emitted
// ---------------------------------------------------------------------------
static void test_sabrina_legal_with_one_bench()
{
    using namespace ptcgp_sim;

    Card sabrina = make_trainer("A1", 225, "Sabrina", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);
    Card bench   = make_pokemon("A1", 2, "Bench", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(sabrina);

    // Give opponent a bench Pokemon
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    auto moves = generate_legal_moves(gs, 0);

    REQUIRE(count_action_type(moves, ActionType::PlaySupporter) == 1);
    // The PlaySupporter for Sabrina should have no slot (slot_index == -1)
    bool found = false;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == sabrina.id && m.slot_index == -1)
            found = true;
    REQUIRE(found);

    std::cout << "  [PASS] test_sabrina_legal_with_one_bench\n";
}

// ---------------------------------------------------------------------------
// Test S2: Sabrina in hand, opponent bench empty -> no PlaySupporter
// ---------------------------------------------------------------------------
static void test_sabrina_illegal_when_bench_empty()
{
    using namespace ptcgp_sim;

    Card sabrina = make_trainer("A1", 225, "Sabrina", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(sabrina);
    // No bench for opponent

    auto moves = generate_legal_moves(gs, 0);

    bool has_sabrina = false;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == sabrina.id)
            has_sabrina = true;
    REQUIRE(!has_sabrina);

    std::cout << "  [PASS] test_sabrina_illegal_when_bench_empty\n";
}

// ---------------------------------------------------------------------------
// Test S4: Sabrina response window -> opponent gets ChooseBenchSlot options only
// ---------------------------------------------------------------------------
static void test_sabrina_response_window_opponent_gets_choose_bench_slot()
{
    using namespace ptcgp_sim;

    Card active = make_pokemon("A1", 1, "Active", 60);
    Card bench1 = make_pokemon("A1", 2, "Bench1", 60);
    Card bench2 = make_pokemon("A1", 3, "Bench2", 60);

    GameState gs = make_game(active, active);

    // Set up opponent bench
    InPlayPokemon b1; b1.card = bench1; b1.played_this_turn = false;
    InPlayPokemon b2; b2.card = bench2; b2.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = b1;
    gs.players[1].pokemon_slots[2] = b2;

    // Simulate Sabrina having been played: set pending response
    gs.pending_response        = PendingResponse::SabrinaChoice;
    gs.pending_response_player = 1; // opponent is player 1

    // Opponent (player 1) should get exactly 2 ChooseBenchSlot actions
    auto opp_moves = generate_legal_moves(gs, 1);
    REQUIRE(count_action_type(opp_moves, ActionType::ChooseBenchSlot) == 2);
    REQUIRE(!has_action_type(opp_moves, ActionType::Pass));
    REQUIRE(!has_action_type(opp_moves, ActionType::PlayPokemon));
    REQUIRE(!has_action_type(opp_moves, ActionType::AttachEnergy));

    // Active player (player 0) should get NO moves
    auto active_moves = generate_legal_moves(gs, 0);
    REQUIRE(active_moves.empty());

    std::cout << "  [PASS] test_sabrina_response_window_opponent_gets_choose_bench_slot\n";
}

// ---------------------------------------------------------------------------
// Test S5: Playing Sabrina sets pending_response and supporter_played_this_turn
// ---------------------------------------------------------------------------
static void test_sabrina_play_sets_pending_response()
{
    using namespace ptcgp_sim;

    Card sabrina = make_trainer("A1", 225, "Sabrina", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);
    Card bench   = make_pokemon("A1", 2, "Bench", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(sabrina);

    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(sabrina.id), rng);

    REQUIRE(gs.supporter_played_this_turn == true);
    REQUIRE(gs.pending_response == PendingResponse::SabrinaChoice);
    REQUIRE(gs.pending_response_player == 1);
    // Sabrina should be in discard pile
    bool in_discard = std::any_of(gs.players[0].discard_pile.begin(),
                                   gs.players[0].discard_pile.end(),
                                   [](const Card& c){ return c.name == "Sabrina"; });
    REQUIRE(in_discard);

    std::cout << "  [PASS] test_sabrina_play_sets_pending_response\n";
}

// ---------------------------------------------------------------------------
// Test S6: ChooseBenchSlot swaps correctly and clears pending_response
// ---------------------------------------------------------------------------
static void test_choose_bench_slot_swaps_and_clears_pending()
{
    using namespace ptcgp_sim;

    Card active_card = make_pokemon("A1", 1, "ActiveMon", 60);
    Card bench_card  = make_pokemon("A1", 2, "BenchMon", 80);

    GameState gs = make_game(active_card, active_card);

    // Set up opponent (player 1) with a bench Pokemon
    InPlayPokemon bench_ip; bench_ip.card = bench_card; bench_ip.played_this_turn = false;
    bench_ip.attached_energy.push_back(EnergyType::Fire);
    gs.players[1].pokemon_slots[1] = bench_ip;

    // Simulate Sabrina pending response
    gs.pending_response        = PendingResponse::SabrinaChoice;
    gs.pending_response_player = 1;

    std::mt19937 rng(42);
    apply_action(gs, Action::choose_bench_slot(1), rng);

    // BenchMon should now be active
    REQUIRE(gs.players[1].pokemon_slots[0].has_value());
    REQUIRE(gs.players[1].pokemon_slots[0]->card.name == "BenchMon");
    // Energy preserved
    REQUIRE(gs.players[1].pokemon_slots[0]->attached_energy.size() == 1);
    REQUIRE(gs.players[1].pokemon_slots[0]->attached_energy[0] == EnergyType::Fire);

    // Old active should now be on bench slot 1
    REQUIRE(gs.players[1].pokemon_slots[1].has_value());
    REQUIRE(gs.players[1].pokemon_slots[1]->card.name == "ActiveMon");

    // pending_response cleared
    REQUIRE(gs.pending_response == PendingResponse::None);
    REQUIRE(gs.pending_response_player == -1);

    std::cout << "  [PASS] test_choose_bench_slot_swaps_and_clears_pending\n";
}

// ---------------------------------------------------------------------------
// Test S7: Sabrina swap clears volatile status on the retreating active
// ---------------------------------------------------------------------------
static void test_sabrina_swap_clears_volatile_status()
{
    using namespace ptcgp_sim;

    Card active_card = make_pokemon("A1", 1, "ActiveMon", 60);
    Card bench_card  = make_pokemon("A1", 2, "BenchMon", 80);

    GameState gs = make_game(active_card, active_card);

    // Give opponent's active a volatile status
    gs.players[1].pokemon_slots[0]->status = StatusCondition::Paralyzed;

    InPlayPokemon bench_ip; bench_ip.card = bench_card; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    gs.pending_response        = PendingResponse::SabrinaChoice;
    gs.pending_response_player = 1;

    std::mt19937 rng(42);
    apply_action(gs, Action::choose_bench_slot(1), rng);

    // The old active (now on bench) should have its volatile status cleared
    REQUIRE(gs.players[1].pokemon_slots[1].has_value());
    REQUIRE(gs.players[1].pokemon_slots[1]->status == StatusCondition::None);

    std::cout << "  [PASS] test_sabrina_swap_clears_volatile_status\n";
}

// ============================================================================
// Cyrus move-generation tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test C1: Cyrus in hand, opponent has 1 damaged bench -> 1 PlaySupporter
// ---------------------------------------------------------------------------
static void test_cyrus_legal_with_damaged_bench()
{
    using namespace ptcgp_sim;

    Card cyrus  = make_trainer("A2", 150, "Cyrus", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card bench  = make_pokemon("A1", 2, "Bench", 80);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(cyrus);

    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    bench_ip.damage_counters = 20; // damaged
    gs.players[1].pokemon_slots[1] = bench_ip;

    auto moves = generate_legal_moves(gs, 0);

    // Should have exactly 1 PlaySupporter for Cyrus targeting slot 1
    int cyrus_moves = 0;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == cyrus.id)
            ++cyrus_moves;
    REQUIRE(cyrus_moves == 1);

    // Verify the slot_index is set correctly
    bool correct_slot = false;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == cyrus.id && m.slot_index == 1)
            correct_slot = true;
    REQUIRE(correct_slot);

    std::cout << "  [PASS] test_cyrus_legal_with_damaged_bench\n";
}

// ---------------------------------------------------------------------------
// Test C2: Cyrus in hand, opponent bench has no damage -> no PlaySupporter
// ---------------------------------------------------------------------------
static void test_cyrus_illegal_when_no_damaged_bench()
{
    using namespace ptcgp_sim;

    Card cyrus  = make_trainer("A2", 150, "Cyrus", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card bench  = make_pokemon("A1", 2, "Bench", 80);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(cyrus);

    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    bench_ip.damage_counters = 0; // undamaged
    gs.players[1].pokemon_slots[1] = bench_ip;

    auto moves = generate_legal_moves(gs, 0);

    bool has_cyrus = false;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == cyrus.id)
            has_cyrus = true;
    REQUIRE(!has_cyrus);

    std::cout << "  [PASS] test_cyrus_illegal_when_no_damaged_bench\n";
}

// ---------------------------------------------------------------------------
// Test C3: Cyrus in hand, opponent bench empty -> no PlaySupporter
// ---------------------------------------------------------------------------
static void test_cyrus_illegal_when_bench_empty()
{
    using namespace ptcgp_sim;

    Card cyrus  = make_trainer("A2", 150, "Cyrus", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(cyrus);
    // No bench for opponent

    auto moves = generate_legal_moves(gs, 0);

    bool has_cyrus = false;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == cyrus.id)
            has_cyrus = true;
    REQUIRE(!has_cyrus);

    std::cout << "  [PASS] test_cyrus_illegal_when_bench_empty\n";
}

// ---------------------------------------------------------------------------
// Test C4: Cyrus with multiple damaged bench -> one action per damaged slot
// ---------------------------------------------------------------------------
static void test_cyrus_multiple_damaged_bench_slots()
{
    using namespace ptcgp_sim;

    Card cyrus  = make_trainer("A2", 150, "Cyrus", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card bench  = make_pokemon("A1", 2, "Bench", 80);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(cyrus);

    // Two damaged bench Pokemon, one undamaged
    InPlayPokemon b1; b1.card = bench; b1.damage_counters = 10; b1.played_this_turn = false;
    InPlayPokemon b2; b2.card = bench; b2.damage_counters = 0;  b2.played_this_turn = false;
    InPlayPokemon b3; b3.card = bench; b3.damage_counters = 30; b3.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = b1;
    gs.players[1].pokemon_slots[2] = b2;
    gs.players[1].pokemon_slots[3] = b3;

    auto moves = generate_legal_moves(gs, 0);

    int cyrus_count = 0;
    for (const auto& m : moves)
        if (m.type == ActionType::PlaySupporter && m.card_id == cyrus.id)
            ++cyrus_count;
    REQUIRE(cyrus_count == 2); // slots 1 and 3 are damaged

    std::cout << "  [PASS] test_cyrus_multiple_damaged_bench_slots\n";
}

// ---------------------------------------------------------------------------
// Test C5: Playing Cyrus swaps bench and active, clears volatile status
// ---------------------------------------------------------------------------
static void test_cyrus_play_swaps_bench_and_active()
{
    using namespace ptcgp_sim;

    Card cyrus       = make_trainer("A2", 150, "Cyrus", TrainerType::Supporter);
    Card active_card = make_pokemon("A1", 1, "ActiveMon", 60);
    Card bench_card  = make_pokemon("A1", 2, "BenchMon", 80);

    GameState gs = make_game(active_card, active_card);
    gs.players[0].hand.push_back(cyrus);

    // Opponent: active has Confused status, bench slot 2 has 20 damage
    gs.players[1].pokemon_slots[0]->status = StatusCondition::Confused;
    gs.players[1].pokemon_slots[0]->attached_energy.push_back(EnergyType::Water);

    InPlayPokemon bench_ip; bench_ip.card = bench_card; bench_ip.played_this_turn = false;
    bench_ip.damage_counters = 20;
    bench_ip.attached_energy.push_back(EnergyType::Fire);
    gs.players[1].pokemon_slots[2] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(cyrus.id, 2), rng);

    // BenchMon should now be active
    REQUIRE(gs.players[1].pokemon_slots[0].has_value());
    REQUIRE(gs.players[1].pokemon_slots[0]->card.name == "BenchMon");
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 20);
    REQUIRE(gs.players[1].pokemon_slots[0]->attached_energy.size() == 1);
    REQUIRE(gs.players[1].pokemon_slots[0]->attached_energy[0] == EnergyType::Fire);

    // Old active (ActiveMon) should now be on bench slot 2
    REQUIRE(gs.players[1].pokemon_slots[2].has_value());
    REQUIRE(gs.players[1].pokemon_slots[2]->card.name == "ActiveMon");
    // Volatile status cleared
    REQUIRE(gs.players[1].pokemon_slots[2]->status == StatusCondition::None);
    // Energy preserved
    REQUIRE(gs.players[1].pokemon_slots[2]->attached_energy.size() == 1);
    REQUIRE(gs.players[1].pokemon_slots[2]->attached_energy[0] == EnergyType::Water);

    // supporter_played_this_turn set
    REQUIRE(gs.supporter_played_this_turn == true);
    // Cyrus in discard
    bool in_discard = std::any_of(gs.players[0].discard_pile.begin(),
                                   gs.players[0].discard_pile.end(),
                                   [](const Card& c){ return c.name == "Cyrus"; });
    REQUIRE(in_discard);
    // No pending response for Cyrus
    REQUIRE(gs.pending_response == PendingResponse::None);

    std::cout << "  [PASS] test_cyrus_play_swaps_bench_and_active\n";
}

// ---------------------------------------------------------------------------
// Test C6: Swap preserves damage counters, tools, and cards_behind
// ---------------------------------------------------------------------------
static void test_swap_preserves_all_state()
{
    using namespace ptcgp_sim;

    Card active_card = make_pokemon("A1", 1, "ActiveMon", 100);
    Card bench_card  = make_pokemon("A1", 2, "BenchMon", 80);
    Card tool_card   = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs = make_game(active_card, active_card);

    // Opponent active: 30 damage, tool attached
    gs.players[1].pokemon_slots[0]->damage_counters = 30;
    gs.players[1].pokemon_slots[0]->attached_tool   = tool_card;

    // Opponent bench slot 1: 50 damage, 2 energies
    InPlayPokemon bench_ip; bench_ip.card = bench_card; bench_ip.played_this_turn = false;
    bench_ip.damage_counters = 50;
    bench_ip.attached_energy.push_back(EnergyType::Psychic);
    bench_ip.attached_energy.push_back(EnergyType::Psychic);
    gs.players[1].pokemon_slots[1] = bench_ip;

    // Simulate Sabrina pending response
    gs.pending_response        = PendingResponse::SabrinaChoice;
    gs.pending_response_player = 1;

    std::mt19937 rng(42);
    apply_action(gs, Action::choose_bench_slot(1), rng);

    // BenchMon now active: 50 damage, 2 energies preserved
    REQUIRE(gs.players[1].pokemon_slots[0]->card.name == "BenchMon");
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 50);
    REQUIRE(gs.players[1].pokemon_slots[0]->attached_energy.size() == 2);

    // ActiveMon now on bench: 30 damage, tool preserved
    REQUIRE(gs.players[1].pokemon_slots[1]->card.name == "ActiveMon");
    REQUIRE(gs.players[1].pokemon_slots[1]->damage_counters == 30);
    REQUIRE(gs.players[1].pokemon_slots[1]->attached_tool.has_value());
    REQUIRE(gs.players[1].pokemon_slots[1]->attached_tool->name == "Rocky Helmet");

    std::cout << "  [PASS] test_swap_preserves_all_state\n";
}

// ============================================================================
// Professor's Research tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test PR1: Professor's Research draws 2 cards from deck
// ---------------------------------------------------------------------------
static void test_professors_research_draws_two_cards()
{
    using namespace ptcgp_sim;

    Card prof_research = make_trainer("A4b", 373, "Professor's Research", TrainerType::Supporter);
    Card active        = make_pokemon("A1", 1, "Active", 60);
    Card deck_card     = make_pokemon("A1", 2, "DeckMon", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(prof_research);
    // Give player 0 a deck with 5 cards
    gs.players[0].deck.cards = std::vector<Card>(5, deck_card);

    const std::size_t hand_before = gs.players[0].hand.size(); // 1 (prof research)
    const std::size_t deck_before = gs.players[0].deck.cards.size(); // 5

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(prof_research.id), rng);

    // Card removed from hand, 2 drawn: net hand size = hand_before - 1 + 2
    REQUIRE(gs.players[0].hand.size() == hand_before - 1 + 2);
    REQUIRE(gs.players[0].deck.cards.size() == deck_before - 2);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_professors_research_draws_two_cards\n";
}

// ---------------------------------------------------------------------------
// Test PR2: Professor's Research with only 1 deck card draws 1 without error
// ---------------------------------------------------------------------------
static void test_professors_research_draws_partial_when_deck_small()
{
    using namespace ptcgp_sim;

    Card prof_research = make_trainer("P-A", 7, "Professor's Research", TrainerType::Supporter);
    Card active        = make_pokemon("A1", 1, "Active", 60);
    Card deck_card     = make_pokemon("A1", 2, "DeckMon", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(prof_research);
    gs.players[0].deck.cards = std::vector<Card>(1, deck_card);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(prof_research.id), rng);

    // Only 1 card in deck, so only 1 drawn; prof research removed from hand
    REQUIRE(gs.players[0].hand.size() == 1); // 0 remaining + 1 drawn
    REQUIRE(gs.players[0].deck.cards.empty());

    std::cout << "  [PASS] test_professors_research_draws_partial_when_deck_small\n";
}

// ============================================================================
// Giovanni tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test G1: Giovanni sets attack_boost to 10
// ---------------------------------------------------------------------------
static void test_giovanni_sets_attack_boost()
{
    using namespace ptcgp_sim;

    Card giovanni = make_trainer("A1", 223, "Giovanni", TrainerType::Supporter);
    Card active   = make_pokemon("A1", 1, "Active", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(giovanni);

    REQUIRE(gs.attack_boost == 0);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(giovanni.id), rng);

    REQUIRE(gs.attack_boost == 10);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_giovanni_sets_attack_boost\n";
}

// ---------------------------------------------------------------------------
// Test G2: Giovanni causes attack to deal +10 extra damage
// ---------------------------------------------------------------------------
static void test_giovanni_attack_deals_extra_damage()
{
    using namespace ptcgp_sim;

    Card giovanni  = make_trainer("A1", 223, "Giovanni", TrainerType::Supporter);
    Attack scratch = make_attack("Scratch", 30, {EnergyType::Colorless});
    Card attacker  = make_pokemon("A1", 1, "Attacker", 60, EnergyType::Colorless, 0, {scratch});
    Card defender  = make_pokemon("A1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    gs.players[0].hand.push_back(giovanni);
    gs.players[0].pokemon_slots[0]->attached_energy.push_back(EnergyType::Colorless);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(giovanni.id), rng);
    apply_action(gs, Action::attack(0), rng);

    // 30 base + 10 giovanni = 40 damage
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 40);

    std::cout << "  [PASS] test_giovanni_attack_deals_extra_damage\n";
}

// ---------------------------------------------------------------------------
// Test G3: attack_boost resets to 0 after reset_turn_flags
// ---------------------------------------------------------------------------
static void test_giovanni_attack_boost_resets_each_turn()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.attack_boost = 10;
    gs.reset_turn_flags();

    REQUIRE(gs.attack_boost == 0);

    std::cout << "  [PASS] test_giovanni_attack_boost_resets_each_turn\n";
}

// ============================================================================
// Leaf tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test L1: Leaf sets retreat_reduction to 2
// ---------------------------------------------------------------------------
static void test_leaf_sets_retreat_reduction()
{
    using namespace ptcgp_sim;

    Card leaf   = make_trainer("A1a", 68, "Leaf", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(leaf);

    REQUIRE(gs.retreat_reduction == 0);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(leaf.id), rng);

    REQUIRE(gs.retreat_reduction == 2);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_leaf_sets_retreat_reduction\n";
}

// ---------------------------------------------------------------------------
// Test L2: Leaf allows retreating a Pokemon with cost 2 for free
// ---------------------------------------------------------------------------
static void test_leaf_allows_free_retreat_for_cost_2()
{
    using namespace ptcgp_sim;

    Card leaf = make_trainer("A1a", 68, "Leaf", TrainerType::Supporter);

    // Active Pokemon with retreat cost 2
    Card heavy = make_pokemon("A1", 1, "Heavy", 100);
    heavy.retreat_cost = {EnergyType::Colorless, EnergyType::Colorless};

    Card bencher = make_pokemon("A1", 2, "Bencher", 60);

    GameState gs = make_game(heavy, make_pokemon("A1", 3, "Opp", 60));
    gs.players[0].hand.push_back(leaf);

    // Give active 1 energy (not enough to retreat normally, cost is 2)
    gs.players[0].pokemon_slots[0]->attached_energy.push_back(EnergyType::Fire);

    // Place a bench Pokemon
    InPlayPokemon bench_ip; bench_ip.card = bencher; bench_ip.played_this_turn = false;
    gs.players[0].pokemon_slots[1] = bench_ip;

    // Without Leaf, retreat should not be legal (only 1 energy, cost 2)
    auto moves_before = generate_legal_moves(gs, 0);
    bool can_retreat_before = std::any_of(moves_before.begin(), moves_before.end(),
        [](const ptcgp_sim::Action& a){ return a.type == ptcgp_sim::ActionType::Retreat; });
    REQUIRE(!can_retreat_before);

    // Play Leaf
    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(leaf.id), rng);

    // Now retreat should be legal (cost 2 - 2 = 0)
    auto moves_after = generate_legal_moves(gs, 0);
    bool can_retreat_after = std::any_of(moves_after.begin(), moves_after.end(),
        [](const ptcgp_sim::Action& a){ return a.type == ptcgp_sim::ActionType::Retreat; });
    REQUIRE(can_retreat_after);

    // Execute retreat — no energy should be discarded
    apply_action(gs, Action::retreat(1), rng);
    REQUIRE(gs.players[0].energy_discard.empty());
    REQUIRE(gs.players[0].pokemon_slots[0]->card.name == "Bencher");

    std::cout << "  [PASS] test_leaf_allows_free_retreat_for_cost_2\n";
}

// ---------------------------------------------------------------------------
// Test L3: retreat_reduction resets to 0 after reset_turn_flags
// ---------------------------------------------------------------------------
static void test_leaf_retreat_reduction_resets_each_turn()
{
    using namespace ptcgp_sim;

    GameState gs;
    gs.retreat_reduction = 2;
    gs.reset_turn_flags();

    REQUIRE(gs.retreat_reduction == 0);

    std::cout << "  [PASS] test_leaf_retreat_reduction_resets_each_turn\n";
}

// ============================================================================
// Mars tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test M1: Mars clears opponent hand and redraws (3 - points) cards
// ---------------------------------------------------------------------------
static void test_mars_reshuffles_opponent_hand_and_redraws()
{
    using namespace ptcgp_sim;

    Card mars   = make_trainer("A2", 155, "Mars", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card filler = make_pokemon("A1", 2, "Filler", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(mars);

    // Opponent has 4 cards in hand and 10 in deck, 1 point scored
    gs.players[1].hand = std::vector<Card>(4, filler);
    gs.players[1].deck.cards = std::vector<Card>(10, filler);
    gs.players[1].points = 1;

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(mars.id), rng);

    // Opponent should have (3 - 1) = 2 cards in hand
    REQUIRE(gs.players[1].hand.size() == 2);
    // Opponent deck should have 10 + 4 - 2 = 12 cards
    REQUIRE(gs.players[1].deck.cards.size() == 12);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_mars_reshuffles_opponent_hand_and_redraws\n";
}

// ---------------------------------------------------------------------------
// Test M2: Mars with opponent at 0 points draws 3 cards
// ---------------------------------------------------------------------------
static void test_mars_draws_three_when_opponent_at_zero_points()
{
    using namespace ptcgp_sim;

    Card mars   = make_trainer("A2", 155, "Mars", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card filler = make_pokemon("A1", 2, "Filler", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(mars);

    gs.players[1].hand = std::vector<Card>(5, filler);
    gs.players[1].deck.cards = std::vector<Card>(10, filler);
    gs.players[1].points = 0;

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(mars.id), rng);

    // (3 - 0) = 3 cards drawn
    REQUIRE(gs.players[1].hand.size() == 3);

    std::cout << "  [PASS] test_mars_draws_three_when_opponent_at_zero_points\n";
}

// ============================================================================
// Copycat tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test CC1: Copycat shuffles hand and draws equal to opponent's hand size
// ---------------------------------------------------------------------------
static void test_copycat_draws_equal_to_opponent_hand_size()
{
    using namespace ptcgp_sim;

    Card copycat = make_trainer("B1", 225, "Copycat", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);
    Card filler  = make_pokemon("A1", 2, "Filler", 60);

    GameState gs = make_game(active, active);
    // Player 0 hand: copycat + 2 other cards
    gs.players[0].hand.push_back(copycat);
    gs.players[0].hand.push_back(filler);
    gs.players[0].hand.push_back(filler);
    gs.players[0].deck.cards = std::vector<Card>(10, filler);

    // Opponent has 5 cards in hand
    gs.players[1].hand = std::vector<Card>(5, filler);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(copycat.id), rng);

    // Player 0 should now have 5 cards (opponent's hand size at time of play)
    REQUIRE(gs.players[0].hand.size() == 5);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_copycat_draws_equal_to_opponent_hand_size\n";
}

// ---------------------------------------------------------------------------
// Test CC2: Copycat with opponent having 0 cards results in empty hand
// ---------------------------------------------------------------------------
static void test_copycat_with_empty_opponent_hand()
{
    using namespace ptcgp_sim;

    Card copycat = make_trainer("B1", 225, "Copycat", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);
    Card filler  = make_pokemon("A1", 2, "Filler", 60);

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(copycat);
    gs.players[0].hand.push_back(filler);
    gs.players[0].deck.cards = std::vector<Card>(10, filler);

    // Opponent has 0 cards in hand
    gs.players[1].hand.clear();

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(copycat.id), rng);

    // Player 0 draws 0 cards
    REQUIRE(gs.players[0].hand.empty());

    std::cout << "  [PASS] test_copycat_with_empty_opponent_hand\n";
}

// ============================================================================
// Lisia tests
// ============================================================================

// ---------------------------------------------------------------------------
// Test LI1: Lisia fetches 2 random Basic Pokemon with HP <= 50
// ---------------------------------------------------------------------------
static void test_lisia_fetches_two_small_basics()
{
    using namespace ptcgp_sim;

    Card lisia   = make_trainer("B1", 226, "Lisia", TrainerType::Supporter);
    Card active  = make_pokemon("A1", 1, "Active", 60);
    Card small1  = make_pokemon("A1", 10, "Small1", 40);  // qualifies
    Card small2  = make_pokemon("A1", 11, "Small2", 50);  // qualifies
    Card big     = make_pokemon("A1", 12, "Big",    80);  // does not qualify

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(lisia);
    gs.players[0].deck.cards = {small1, small2, big, big, big};

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(lisia.id), rng);

    // Should have drawn both small basics (lisia removed, 2 drawn)
    REQUIRE(gs.players[0].hand.size() == 2);
    bool has_small1 = std::any_of(gs.players[0].hand.begin(), gs.players[0].hand.end(),
        [](const Card& c){ return c.name == "Small1"; });
    bool has_small2 = std::any_of(gs.players[0].hand.begin(), gs.players[0].hand.end(),
        [](const Card& c){ return c.name == "Small2"; });
    REQUIRE(has_small1);
    REQUIRE(has_small2);
    REQUIRE(gs.supporter_played_this_turn == true);

    std::cout << "  [PASS] test_lisia_fetches_two_small_basics\n";
}

// ---------------------------------------------------------------------------
// Test LI2: Lisia with no qualifying Pokemon does nothing (no crash)
// ---------------------------------------------------------------------------
static void test_lisia_no_qualifying_pokemon_no_crash()
{
    using namespace ptcgp_sim;

    Card lisia  = make_trainer("B1", 226, "Lisia", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card big    = make_pokemon("A1", 2, "Big", 80); // does not qualify

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(lisia);
    gs.players[0].deck.cards = std::vector<Card>(5, big);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(lisia.id), rng);

    // Lisia removed from hand, nothing drawn
    REQUIRE(gs.players[0].hand.empty());

    std::cout << "  [PASS] test_lisia_no_qualifying_pokemon_no_crash\n";
}

// ---------------------------------------------------------------------------
// Test LI3: Lisia with only 1 qualifying Pokemon draws exactly 1
// ---------------------------------------------------------------------------
static void test_lisia_draws_one_when_only_one_qualifies()
{
    using namespace ptcgp_sim;

    Card lisia  = make_trainer("B1", 226, "Lisia", TrainerType::Supporter);
    Card active = make_pokemon("A1", 1, "Active", 60);
    Card small  = make_pokemon("A1", 2, "Small", 50); // qualifies
    Card big    = make_pokemon("A1", 3, "Big",   80); // does not qualify

    GameState gs = make_game(active, active);
    gs.players[0].hand.push_back(lisia);
    gs.players[0].deck.cards = {small, big, big};

    std::mt19937 rng(42);
    apply_action(gs, Action::play_supporter(lisia.id), rng);

    REQUIRE(gs.players[0].hand.size() == 1);
    REQUIRE(gs.players[0].hand[0].name == "Small");

    std::cout << "  [PASS] test_lisia_draws_one_when_only_one_qualifies\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== ptcgp_sim supporter effects tests ===\n";

    // Sabrina move generation
    RUN_TEST(test_sabrina_legal_with_one_bench);
    RUN_TEST(test_sabrina_illegal_when_bench_empty);
    RUN_TEST(test_sabrina_response_window_opponent_gets_choose_bench_slot);

    // Sabrina effect resolution
    RUN_TEST(test_sabrina_play_sets_pending_response);
    RUN_TEST(test_choose_bench_slot_swaps_and_clears_pending);
    RUN_TEST(test_sabrina_swap_clears_volatile_status);

    // Cyrus move generation
    RUN_TEST(test_cyrus_legal_with_damaged_bench);
    RUN_TEST(test_cyrus_illegal_when_no_damaged_bench);
    RUN_TEST(test_cyrus_illegal_when_bench_empty);
    RUN_TEST(test_cyrus_multiple_damaged_bench_slots);

    // Cyrus effect resolution
    RUN_TEST(test_cyrus_play_swaps_bench_and_active);
    RUN_TEST(test_swap_preserves_all_state);

    // Professor's Research
    RUN_TEST(test_professors_research_draws_two_cards);
    RUN_TEST(test_professors_research_draws_partial_when_deck_small);

    // Giovanni
    RUN_TEST(test_giovanni_sets_attack_boost);
    RUN_TEST(test_giovanni_attack_deals_extra_damage);
    RUN_TEST(test_giovanni_attack_boost_resets_each_turn);

    // Leaf
    RUN_TEST(test_leaf_sets_retreat_reduction);
    RUN_TEST(test_leaf_allows_free_retreat_for_cost_2);
    RUN_TEST(test_leaf_retreat_reduction_resets_each_turn);

    // Mars
    RUN_TEST(test_mars_reshuffles_opponent_hand_and_redraws);
    RUN_TEST(test_mars_draws_three_when_opponent_at_zero_points);

    // Copycat
    RUN_TEST(test_copycat_draws_equal_to_opponent_hand_size);
    RUN_TEST(test_copycat_with_empty_opponent_hand);

    // Lisia
    RUN_TEST(test_lisia_fetches_two_small_basics);
    RUN_TEST(test_lisia_no_qualifying_pokemon_no_crash);
    RUN_TEST(test_lisia_draws_one_when_only_one_qualifies);

    return ptcgp_test::print_summary();
}
