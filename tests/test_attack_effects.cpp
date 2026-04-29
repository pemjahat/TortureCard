// Unit tests for Mechanic dispatch in apply_attack_damage (Requirements 1-6
// of the Attack Effect Mechanic System plan).
//
// Build target: ptcgp_test_attack_effects (added in CMakeLists.txt)

#include "test_helpers.h"

#include "ptcgp_sim/effects.h"
#include "ptcgp_sim/attack_mechanic.h"
#include "test_support.h"

#include <memory>

// ---------------------------------------------------------------------------
// Local helpers (attack-test-specific)
// ---------------------------------------------------------------------------

static ptcgp_sim::Attack make_attack_with_mechanic(
    const std::string& name,
    int damage,
    std::unique_ptr<ptcgp_sim::AttackMechanic> mechanic)
{
    ptcgp_sim::Attack a;
    a.name     = name;
    a.damage   = damage;
    a.mechanic = std::move(mechanic);
    return a;
}

static ptcgp_sim::Attack make_basic_attack(const std::string& name, int damage)
{
    ptcgp_sim::Attack a;
    a.name   = name;
    a.damage = damage;
    return a;
}

// ============================================================================
// Requirement 1: Mechanic variant type
// ============================================================================

static void test_mechanic_equality()
{
    using namespace ptcgp_sim;

    BasicDamage bd1, bd2;
    REQUIRE(bd1 == bd2);

    FlipNCoinDamage fnd_a{1, 30, 0};
    FlipNCoinDamage fnd_b{1, 30, 0};
    FlipNCoinDamage fnd_c{1, 20, 0};
    REQUIRE(fnd_a == fnd_b);
    REQUIRE(!(fnd_a == fnd_c));

    FlipNCoinExtraDamage fned_a{2, 20, true};
    FlipNCoinExtraDamage fned_b{2, 20, true};
    FlipNCoinExtraDamage fned_c{2, 20, false};
    REQUIRE(fned_a == fned_b);
    REQUIRE(!(fned_a == fned_c));

    SelfHeal sh_a{30};
    SelfHeal sh_b{30};
    SelfHeal sh_c{20};
    REQUIRE(sh_a == sh_b);
    REQUIRE(!(sh_a == sh_c));

    FlipUntilTailsDamage futd{20};
    FlipUntilTailsDamage futd2{20};
    REQUIRE(futd == futd2);

    // Cross-type: different concrete types are never equal
    REQUIRE(!(bd1 == fnd_a));
    REQUIRE(!(sh_a == fnd_a));
}

// ============================================================================
// Requirement 5: apply_attack_damage dispatches on Mechanic
// ============================================================================

// ---------------------------------------------------------------------------
// R5-1: nullopt mechanic -> fixed damage only (no regression)
// ---------------------------------------------------------------------------
static void test_no_mechanic_uses_fixed_damage()
{
    using namespace ptcgp_sim;

    Attack atk = make_basic_attack("Tackle", 30);
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(42);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 30);
}

// ---------------------------------------------------------------------------
// R5-2: FlipNCoinDamage heads -> heads_damage applied
// ---------------------------------------------------------------------------
static void test_flip1coin_damage_heads()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_heads(1, 1); // 1 coin, 1 head
    Attack atk = make_attack_with_mechanic("Gnaw", 0, std::make_unique<FlipNCoinDamage>(1, 30, 0));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 30);
}

// ---------------------------------------------------------------------------
// R5-3: FlipNCoinDamage tails -> 0 damage
// ---------------------------------------------------------------------------
static void test_flip1coin_damage_tails()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_heads(1, 0); // 1 coin, 0 heads (tails)
    Attack atk = make_attack_with_mechanic("Gnaw", 0, std::make_unique<FlipNCoinDamage>(1, 30, 0));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 0);
}

// ---------------------------------------------------------------------------
// R5-4: FlipNCoinExtraDamage with include_fixed_damage=true, 2 heads
// ---------------------------------------------------------------------------
static void test_flip2coin_extra_damage_2heads()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_heads(2, 2); // 2 coins, 2 heads
    // attack.damage=40, extra_damage=20, include_fixed=true -> 40 + 20*2 = 80
    Attack atk = make_attack_with_mechanic("Double Slap", 40,
                     std::make_unique<FlipNCoinExtraDamage>(2, 20, true));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 80);
}

// ---------------------------------------------------------------------------
// R5-4b: FlipNCoinExtraDamage with include_fixed_damage=false, 2 heads
// ---------------------------------------------------------------------------
static void test_flip2coin_damage_no_fixed_2heads()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_heads(2, 2); // 2 coins, 2 heads
    // include_fixed=false -> 0 + 30*2 = 60
    Attack atk = make_attack_with_mechanic("Fury Attack", 0,
                     std::make_unique<FlipNCoinExtraDamage>(2, 30, false));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 60);
}

// ---------------------------------------------------------------------------
// R5-5: SelfHeal — attacker heals after dealing damage
// ---------------------------------------------------------------------------
static void test_self_heal_reduces_attacker_damage()
{
    using namespace ptcgp_sim;

    // Attacker has 50 damage counters; attack deals 40 damage and heals 30
    Attack atk = make_attack_with_mechanic("Mega Drain", 40, std::make_unique<SelfHeal>(30));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender, 50 /*p0 damage counters*/);
    std::mt19937 rng(42);
    apply_attack_damage(gs, 0, 0, rng);

    // Defender takes 40 damage
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 40);
    // Attacker healed 30: 50 - 30 = 20
    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 20);
}

// ---------------------------------------------------------------------------
// R5-5b: SelfHeal clamped to 0 (cannot go negative)
// ---------------------------------------------------------------------------
static void test_self_heal_clamped_to_zero()
{
    using namespace ptcgp_sim;

    // Attacker has only 10 damage counters; heals 30 -> should clamp to 0
    Attack atk = make_attack_with_mechanic("Mega Drain", 40, std::make_unique<SelfHeal>(30));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender, 10 /*p0 damage counters*/);
    std::mt19937 rng(42);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[0].pokemon_slots[0]->damage_counters == 0);
}

// ---------------------------------------------------------------------------
// R5-6: FlipUntilTailsDamage — H, H, T -> 2 heads -> 40 damage
// ---------------------------------------------------------------------------
static void test_flip_until_tails_2heads()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_flip_until_tails(2); // H, H, T
    Attack atk = make_attack_with_mechanic("Waterfall", 0, std::make_unique<FlipUntilTailsDamage>(20));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 40);
}

// ---------------------------------------------------------------------------
// R5-6b: FlipUntilTailsDamage — immediate tails -> 0 damage
// ---------------------------------------------------------------------------
static void test_flip_until_tails_0heads()
{
    using namespace ptcgp_sim;

    auto seed = find_seed_for_flip_until_tails(0); // T immediately
    Attack atk = make_attack_with_mechanic("Waterfall", 0, std::make_unique<FlipUntilTailsDamage>(20));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Colorless, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 0);
}

// ---------------------------------------------------------------------------
// R5-7: Weakness is applied after mechanic resolution
// ---------------------------------------------------------------------------
static void test_weakness_applied_after_mechanic()
{
    using namespace ptcgp_sim;

    // Fire attacker vs Water defender with Fire weakness
    auto seed = find_seed_for_heads(1, 1); // 1 coin, heads
    Attack atk = make_attack_with_mechanic("Ember", 0, std::make_unique<FlipNCoinDamage>(1, 30, 0));
    Card attacker = make_pokemon("T1", 1, "Attacker", 100, EnergyType::Fire, {atk});
    Card defender = make_pokemon("T1", 2, "Defender", 100, EnergyType::Water,
                                  {}, EnergyType::Fire /*weakness*/);

    GameState gs = make_game(attacker, defender);
    std::mt19937 rng(seed);
    apply_attack_damage(gs, 0, 0, rng);

    // 30 (heads) + 20 (weakness) = 50
    REQUIRE(gs.players[1].pokemon_slots[0]->damage_counters == 50);
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== ptcgp_sim attack effect mechanic tests ===\n";

    // Requirement 1: Mechanic variant equality
    RUN_TEST(test_mechanic_equality);

    // Requirement 5: apply_attack_damage dispatch
    RUN_TEST(test_no_mechanic_uses_fixed_damage);
    RUN_TEST(test_flip1coin_damage_heads);
    RUN_TEST(test_flip1coin_damage_tails);
    RUN_TEST(test_flip2coin_extra_damage_2heads);
    RUN_TEST(test_flip2coin_damage_no_fixed_2heads);
    RUN_TEST(test_self_heal_reduces_attacker_damage);
    RUN_TEST(test_self_heal_clamped_to_zero);
    RUN_TEST(test_flip_until_tails_2heads);
    RUN_TEST(test_flip_until_tails_0heads);
    RUN_TEST(test_weakness_applied_after_mechanic);

    return ptcgp_test::print_summary();
}
