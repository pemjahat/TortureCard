// Unit tests for WeightedRandomPlayer.
//
// Build target: ptcgp_test_weighted_random_player (registered in CMakeLists.txt)

#include "test_helpers.h"

#include "ptcgp_sim/weighted_random_player.h"

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_decide_returns_one_of_the_legal_moves()
{
    using namespace ptcgp_sim;

    GameState gs;
    std::vector<Action> moves = {
        Action::pass(),
        Action::attach_energy(EnergyType::Fire, 0),
        Action::attack(0),
    };

    WeightedRandomPlayer player(123);
    Action chosen = player.decide(gs, moves);

    bool found = false;
    for (const Action& move : moves)
    {
        if (move.type == chosen.type &&
            move.target_slot == chosen.target_slot &&
            move.attack_index == chosen.attack_index)
        {
            found = true;
            break;
        }
    }

    REQUIRE(found);
}

static void test_fixed_seed_produces_deterministic_decisions()
{
    using namespace ptcgp_sim;

    GameState gs;
    std::vector<Action> moves = {
        Action::pass(),
        Action::play_pokemon({"A1", 1}, 0),
        Action::attach_energy(EnergyType::Fire, 0),
        Action::attack(0),
        Action::retreat(1),
    };

    WeightedRandomPlayer p0(42);
    WeightedRandomPlayer p1(42);

    for (int i = 0; i < 20; ++i)
    {
        Action a = p0.decide(gs, moves);
        Action b = p1.decide(gs, moves);

        REQUIRE(a.type == b.type);
        REQUIRE(a.card_id == b.card_id);
        REQUIRE(a.slot_index == b.slot_index);
        REQUIRE(a.target_slot == b.target_slot);
        REQUIRE(a.attack_index == b.attack_index);
    }
}

static void test_single_legal_move_is_selected()
{
    using namespace ptcgp_sim;

    GameState gs;
    std::vector<Action> moves = { Action::pass() };

    WeightedRandomPlayer player(7);
    Action chosen = player.decide(gs, moves);

    REQUIRE(chosen.type == ActionType::Pass);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "=== ptcgp_sim weighted random player tests ===\n";

    RUN_TEST(test_decide_returns_one_of_the_legal_moves);
    RUN_TEST(test_fixed_seed_produces_deterministic_decisions);
    RUN_TEST(test_single_legal_move_is_selected);

    std::cout << "\n";
    if (g_failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cerr << g_failures << " test(s) FAILED.\n";

    return g_failures > 0 ? 1 : 0;
}
