// tests/test_toolitem_effects.cpp
// ---------------------------------------------------------------------------
// Unit tests for Rocky Helmet (Tool), Giant Cape (Tool), and Potion (Item).
// Build target: ptcgp_test_toolitem_effects (registered in CMakeLists.txt)
// ---------------------------------------------------------------------------

#include "test_helpers.h"

#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/move_generation.h"
#include "ptcgp_sim/toolitem_effects.h"

// ============================================================================
// Rocky Helmet tests
// ============================================================================

// T1: Normal hit — Rocky Helmet deals 20 damage back to the attacker.
static void test_rocky_helmet_counterattack_normal()
{
    using namespace ptcgp_sim;

    Attack punch = make_attack("Punch", 30);
    Card attacker = make_pokemon("A1", 1, "Attacker", 80,
                                  EnergyType::Colorless, 0, {punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card helmet   = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = helmet;

    // Give bench so game doesn't end on KO
    Card bench = make_pokemon("A1", 3, "Bench", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Defender took 30 damage
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 30);
    // Attacker took 20 counterattack damage from Rocky Helmet
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 20);
}

// T2: Zero-damage attack — Rocky Helmet must NOT trigger.
static void test_rocky_helmet_no_trigger_on_zero_damage()
{
    using namespace ptcgp_sim;

    // Attack with 0 base damage
    Attack zero_punch = make_attack("Zero Punch", 0);
    Card attacker = make_pokemon("A1", 1, "Attacker", 80,
                                  EnergyType::Colorless, 0, {zero_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card helmet   = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = helmet;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // No damage dealt, no counterattack
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 0);
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 0);
}

// T3: Rocky Helmet counterattack damage is capped at card.hp.
static void test_rocky_helmet_counterattack_capped()
{
    using namespace ptcgp_sim;

    Attack punch = make_attack("Punch", 10);
    // Attacker has 100 HP and 90 pre-existing damage — 20 counterattack would
    // exceed hp, so damage_counters must be clamped to 100.
    Card attacker = make_pokemon("A1", 1, "Attacker", 100,
                                  EnergyType::Colorless, 0, {punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card helmet   = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    // p0 starts with 90 damage; 20 counterattack would bring it to 110 → capped at 100
    GameState gs = make_game(attacker, defender, 90 /*p0 damage*/);
    gs.players[1].pokemon_slots[0]->attached_tool = helmet;

    // Give bench for player 0 so game doesn't end when attacker is KO'd
    Card bench = make_pokemon("A1", 3, "Bench", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[0].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Attacker is KO'd (90 + 20 = 110 ≥ 100) — verify it's in the discard pile
    // with damage_counters capped at card.hp (100).
    bool attacker_found = false;
    for (const auto& c : gs.players[0].discard_pile)
    {
        if (c.name == "Attacker") { attacker_found = true; break; }
    }
    REQUIRE(attacker_found);
    // Attacker slot is now nullopt (KO'd and cleared)
    REQUIRE(!gs.players[0].pokemon_slots[0].has_value());
}

// T4: Defender KO'd by attack — Rocky Helmet still fires before KO resolution.
static void test_rocky_helmet_fires_even_when_defender_ko()
{
    using namespace ptcgp_sim;

    Attack ko_punch = make_attack("KO Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 80,
                                  EnergyType::Colorless, 0, {ko_punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card helmet   = make_trainer("A1", 88, "Rocky Helmet", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = helmet;

    // Give bench so game doesn't end
    Card bench = make_pokemon("A1", 3, "Bench", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Defender is KO'd and discarded
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value());
    // Attacker took 20 counterattack damage
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 20);
}

// ============================================================================
// Giant Cape tests
// ============================================================================

// T5: Giant Cape raises the KO threshold by 20.
static void test_giant_cape_raises_ko_threshold()
{
    using namespace ptcgp_sim;

    // Defender has 60 HP + Giant Cape = 80 effective HP
    // Attack deals 60 — would KO without Cape, but not with it.
    Attack punch = make_attack("Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 80,
                                  EnergyType::Colorless, 0, {punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card cape     = make_trainer("A2", 147, "Giant Cape", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = cape;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Defender should still be alive (60 damage < 80 effective HP)
    REQUIRE(gs.players[1].pokemon_slots[0].has_value());
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 60);
    REQUIRE(gs.players[1].pokemon_slots[0]->remaining_hp() == 20);
}

// T6: Without Giant Cape the same attack KOs the defender.
static void test_giant_cape_absent_normal_ko()
{
    using namespace ptcgp_sim;

    Attack punch = make_attack("Punch", 60);
    Card attacker = make_pokemon("A1", 1, "Attacker", 80,
                                  EnergyType::Colorless, 0, {punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);

    GameState gs = make_game(attacker, defender);

    // Give bench so game doesn't end
    Card bench = make_pokemon("A1", 3, "Bench", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Defender is KO'd
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value());
}

// T7: Giant Cape discarded on KO — effective HP reverts to base.
static void test_giant_cape_discarded_on_ko()
{
    using namespace ptcgp_sim;

    // 80 damage exceeds 60 + 20 = 80 effective HP exactly — KO.
    Attack punch = make_attack("Punch", 80);
    Card attacker = make_pokemon("A1", 1, "Attacker", 100,
                                  EnergyType::Colorless, 0, {punch});
    Card defender = make_pokemon("A1", 2, "Defender", 60);
    Card cape     = make_trainer("A2", 147, "Giant Cape", TrainerType::Tool);

    GameState gs = make_game(attacker, defender);
    gs.players[1].pokemon_slots[0]->attached_tool = cape;

    // Give bench so game doesn't end
    Card bench = make_pokemon("A1", 3, "Bench", 60);
    InPlayPokemon bench_ip; bench_ip.card = bench; bench_ip.played_this_turn = false;
    gs.players[1].pokemon_slots[1] = bench_ip;

    std::mt19937 rng(42);
    apply_action(gs, Action::attack(0), rng);

    // Defender KO'd — both Pokemon and Cape in discard
    REQUIRE(!gs.players[1].pokemon_slots[0].has_value());
    bool has_defender = false, has_cape = false;
    for (const auto& c : gs.players[1].discard_pile)
    {
        if (c.name == "Defender")   has_defender = true;
        if (c.name == "Giant Cape") has_cape     = true;
    }
    REQUIRE(has_defender);
    REQUIRE(has_cape);
}

// T8: effective_hp() helper returns base HP when no tool attached.
static void test_effective_hp_no_tool()
{
    using namespace ptcgp_sim;

    Card mon = make_pokemon("A1", 1, "Mon", 70);
    InPlayPokemon ip; ip.card = mon;

    REQUIRE(effective_hp(ip) == 70);
    REQUIRE(ip.max_hp() == 70);
}

// T9: effective_hp() helper returns base HP + 20 when Giant Cape attached.
static void test_effective_hp_with_giant_cape()
{
    using namespace ptcgp_sim;

    Card mon  = make_pokemon("A1", 1, "Mon", 70);
    Card cape = make_trainer("A2", 147, "Giant Cape", TrainerType::Tool);
    InPlayPokemon ip; ip.card = mon; ip.attached_tool = cape;

    REQUIRE(effective_hp(ip) == 90);
    REQUIRE(ip.max_hp() == 90);
    REQUIRE(ip.remaining_hp() == 90);
}

// ============================================================================
// Potion tests
// ============================================================================

// T10: Potion heals 20 damage from the target Pokemon.
static void test_potion_heals_20()
{
    using namespace ptcgp_sim;

    Card mon    = make_pokemon("A1", 1, "Mon", 80);
    Card potion = make_trainer("A1", 196, "Potion", TrainerType::Item);

    GameState gs = make_game(mon, mon, 40 /*p0 damage*/);
    gs.players[0].hand.push_back(potion);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(potion.id, 0), rng);

    // 40 - 20 = 20 damage remaining
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 20);
    // Potion is in discard
    REQUIRE(gs.players[0].discard_pile.size() == 1);
    REQUIRE(gs.players[0].discard_pile[0].name == "Potion");
}

// T11: Potion over-heal clamp — damage_counters cannot go below 0.
static void test_potion_overheal_clamp()
{
    using namespace ptcgp_sim;

    Card mon    = make_pokemon("A1", 1, "Mon", 80);
    Card potion = make_trainer("A1", 196, "Potion", TrainerType::Item);

    // Only 10 damage — healing 20 should clamp to 0
    GameState gs = make_game(mon, mon, 10 /*p0 damage*/);
    gs.players[0].hand.push_back(potion);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(potion.id, 0), rng);

    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 0);
}

// T12: Potion on bench slot heals the correct Pokemon.
static void test_potion_heals_bench_slot()
{
    using namespace ptcgp_sim;

    Card mon    = make_pokemon("A1", 1, "Mon", 80);
    Card potion = make_trainer("A1", 196, "Potion", TrainerType::Item);

    GameState gs = make_game(mon, mon);

    // Place a damaged bench Pokemon in slot 2
    Card bench = make_pokemon("A1", 2, "Bench", 80);
    InPlayPokemon bench_ip; bench_ip.card = bench;
    bench_ip.damage_counters = 30;
    bench_ip.played_this_turn = false;
    gs.players[0].pokemon_slots[2] = bench_ip;

    gs.players[0].hand.push_back(potion);

    std::mt19937 rng(42);
    apply_action(gs, Action::play_item(potion.id, 2), rng);

    // Bench Pokemon healed: 30 - 20 = 10
    REQUIRE(gs.players[0].pokemon_slots[2]->damage_counters == 10);
    // Active untouched
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 0);
}

// T13: Move generation emits one PlayItem per damaged slot for Potion.
static void test_potion_move_generation_per_slot()
{
    using namespace ptcgp_sim;

    Card mon    = make_pokemon("A1", 1, "Mon", 80);
    Card potion = make_trainer("A1", 196, "Potion", TrainerType::Item);

    GameState gs = make_game(mon, mon, 20 /*p0 active damage*/);

    // Add a damaged bench Pokemon
    Card bench = make_pokemon("A1", 2, "Bench", 80);
    InPlayPokemon bench_ip; bench_ip.card = bench;
    bench_ip.damage_counters = 10;
    bench_ip.played_this_turn = false;
    gs.players[0].pokemon_slots[1] = bench_ip;

    gs.players[0].hand.push_back(potion);

    auto moves = generate_legal_moves(gs, 0);

    int potion_moves = 0;
    for (const auto& m : moves)
        if (m.type == ActionType::PlayItem && m.card_id == potion.id)
            ++potion_moves;

    // One action for slot 0 (active, 20 dmg) + one for slot 1 (bench, 10 dmg)
    REQUIRE(potion_moves == 2);
}

// T14: Move generation emits zero PlayItem actions for Potion when all Pokemon are full HP.
static void test_potion_no_moves_when_full_hp()
{
    using namespace ptcgp_sim;

    Card mon    = make_pokemon("A1", 1, "Mon", 80);
    Card potion = make_trainer("A1", 196, "Potion", TrainerType::Item);

    // No damage on either side
    GameState gs = make_game(mon, mon);
    gs.players[0].hand.push_back(potion);

    auto moves = generate_legal_moves(gs, 0);

    for (const auto& m : moves)
        REQUIRE(!(m.type == ActionType::PlayItem && m.card_id == potion.id));
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== Tool & Item Effect Tests ===\n\n";

    std::cout << "-- Rocky Helmet --\n";
    RUN_TEST(test_rocky_helmet_counterattack_normal);
    RUN_TEST(test_rocky_helmet_no_trigger_on_zero_damage);
    RUN_TEST(test_rocky_helmet_counterattack_capped);
    RUN_TEST(test_rocky_helmet_fires_even_when_defender_ko);

    std::cout << "\n-- Giant Cape --\n";
    RUN_TEST(test_giant_cape_raises_ko_threshold);
    RUN_TEST(test_giant_cape_absent_normal_ko);
    RUN_TEST(test_giant_cape_discarded_on_ko);
    RUN_TEST(test_effective_hp_no_tool);
    RUN_TEST(test_effective_hp_with_giant_cape);

    std::cout << "\n-- Potion --\n";
    RUN_TEST(test_potion_heals_20);
    RUN_TEST(test_potion_overheal_clamp);
    RUN_TEST(test_potion_heals_bench_slot);
    RUN_TEST(test_potion_move_generation_per_slot);
    RUN_TEST(test_potion_no_moves_when_full_hp);

    return ptcgp_test::print_summary();
}
