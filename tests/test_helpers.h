// tests/test_helpers.h
// ---------------------------------------------------------------------------
// Shared test infrastructure for all ptcgp_sim unit tests.
//
// Provides:
//   - REQUIRE(expr)  — throws on failure with file/line info
//   - RUN_TEST(func) — runs a test function, prints [PASS]/[FAIL]
//   - g_failures     — global failure counter (one per translation unit)
//   - make_pokemon / make_trainer / make_attack — card factory helpers
//   - make_deck      — deck factory (flat card vector + energy type)
//   - make_game      — GameState factory with both actives pre-placed
//   - find_seed_for_heads / find_seed_for_flip_until_tails — coin-flip RNG helpers
// ---------------------------------------------------------------------------

#pragma once

#include "ptcgp_sim/action.h"
#include "ptcgp_sim/card.h"
#include "ptcgp_sim/deck.h"
#include "ptcgp_sim/game_state.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test infrastructure macros
// ---------------------------------------------------------------------------

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) {                                                         \
            throw std::runtime_error(                                          \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +       \
                " \xe2\x80\x94 REQUIRE failed: " #expr);                       \
        }                                                                      \
    } while (false)

static int g_failures = 0;

#define RUN_TEST(func)                                                         \
    do {                                                                       \
        try {                                                                  \
            func();                                                            \
            std::cout << "  [PASS] " #func "\n";                               \
        } catch (const std::exception& e) {                                    \
            std::cerr << "  [FAIL] " #func "\n"                                \
                      << "         " << e.what() << "\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (false)

// ---------------------------------------------------------------------------
// Card factory helpers
// ---------------------------------------------------------------------------

// Full-featured make_pokemon: covers all test files' signatures.
// Defaults: hp=60, energy=Colorless, stage=0, no attacks, no weakness, no ability.
inline ptcgp_sim::Card make_pokemon(
    const std::string& expansion, int number,
    const std::string& name,
    int hp = 60,
    ptcgp_sim::EnergyType energy_type = ptcgp_sim::EnergyType::Colorless,
    int stage = 0,
    const std::vector<ptcgp_sim::Attack>& attacks = {},
    std::optional<ptcgp_sim::EnergyType> weakness = std::nullopt,
    std::optional<ptcgp_sim::Ability> ability = std::nullopt)
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
    c.ability     = std::move(ability);
    return c;
}

// Trainer card factory.
inline ptcgp_sim::Card make_trainer(
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

// Attack factory.
inline ptcgp_sim::Attack make_attack(
    const std::string& name, int damage,
    std::vector<ptcgp_sim::EnergyType> cost = {})
{
    ptcgp_sim::Attack a;
    a.name            = name;
    a.damage          = damage;
    a.energy_required = std::move(cost);
    return a;
}

// ---------------------------------------------------------------------------
// Deck factory helpers
// ---------------------------------------------------------------------------

// Build a Deck from a flat vector of cards (no database needed).
// Automatically builds the entries list from the card vector.
inline ptcgp_sim::Deck make_deck(
    const std::vector<ptcgp_sim::Card>& cards,
    ptcgp_sim::EnergyType energy = ptcgp_sim::EnergyType::Fire)
{
    ptcgp_sim::Deck d;
    d.energy_types = {energy};
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

// Build a minimal 20-card deck filled with copies of one card.
inline ptcgp_sim::Deck make_uniform_deck(
    const ptcgp_sim::Card& card,
    ptcgp_sim::EnergyType energy = ptcgp_sim::EnergyType::Fire)
{
    return make_deck(std::vector<ptcgp_sim::Card>(20, card), energy);
}

// ---------------------------------------------------------------------------
// GameState factory helper
// ---------------------------------------------------------------------------

// Build a GameState with both actives pre-placed (bypasses setup phase).
// Turn is set to 2 / Action phase so energy attachment is legal.
// Optional damage counters can be pre-applied to either active.
inline ptcgp_sim::GameState make_game(
    const ptcgp_sim::Card& p0_active,
    const ptcgp_sim::Card& p1_active,
    int p0_damage_counters = 0,
    int p1_damage_counters = 0,
    ptcgp_sim::EnergyType deck_energy = ptcgp_sim::EnergyType::Fire)
{
    using namespace ptcgp_sim;
    Card dummy = make_pokemon("XX", 99, "Dummy", 100);
    Deck deck  = make_uniform_deck(dummy, deck_energy);

    GameState gs = GameState::make(deck, deck);
    gs.turn_phase     = TurnPhase::Action;
    gs.turn_number    = 2;
    gs.current_player = 0;

    InPlayPokemon ip0; ip0.card = p0_active; ip0.played_this_turn = false;
    ip0.damage_counters = p0_damage_counters;
    InPlayPokemon ip1; ip1.card = p1_active; ip1.played_this_turn = false;
    ip1.damage_counters = p1_damage_counters;
    gs.players[0].pokemon_slots[0] = ip0;
    gs.players[1].pokemon_slots[0] = ip1;
    return gs;
}

// ---------------------------------------------------------------------------
// Coin-flip RNG seed helpers (used by attack mechanic tests)
// ---------------------------------------------------------------------------

// Find the first seed that produces exactly `target_heads` out of `num_coins`
// flips with std::bernoulli_distribution(0.5).
inline std::mt19937::result_type find_seed_for_heads(int num_coins, int target_heads)
{
    for (std::mt19937::result_type seed = 0; seed < 100000; ++seed)
    {
        std::mt19937 rng(seed);
        std::bernoulli_distribution coin(0.5);
        int h = 0;
        for (int i = 0; i < num_coins; ++i)
            if (coin(rng)) ++h;
        if (h == target_heads) return seed;
    }
    throw std::runtime_error("find_seed_for_heads: no seed found");
}

// Find the first seed that produces exactly `target_heads` heads before the
// first tails in a flip-until-tails sequence.
inline std::mt19937::result_type find_seed_for_flip_until_tails(int target_heads)
{
    for (std::mt19937::result_type seed = 0; seed < 100000; ++seed)
    {
        std::mt19937 rng(seed);
        std::bernoulli_distribution coin(0.5);
        int h = 0;
        while (coin(rng)) ++h;
        if (h == target_heads) return seed;
    }
    throw std::runtime_error("find_seed_for_flip_until_tails: no seed found");
}

// ---------------------------------------------------------------------------
// Test summary helper
// ---------------------------------------------------------------------------

namespace ptcgp_test
{
    inline int print_summary()
    {
        if (g_failures == 0)
            std::cout << "\nAll tests passed.\n";
        else
            std::cerr << "\n" << g_failures << " test(s) FAILED.\n";
        return g_failures == 0 ? 0 : 1;
    }
} // namespace ptcgp_test
