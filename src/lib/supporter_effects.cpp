#include "ptcgp_sim/supporter_effects.h"
#include "ptcgp_sim/game_state.h"
#include "ptcgp_sim/action.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Card identity predicates
// ---------------------------------------------------------------------------

// A1 225 = Sabrina (Genetic Apex)
// A1 272 = Sabrina (Genetic Apex reprint)
static bool is_sabrina(const CardId& id)
{
    if (id.expansion == "A1" && id.number == 225) return true;
    if (id.expansion == "A1" && id.number == 272) return true;
    return false;
}

// A2 150 = Cyrus (Space-Time Smackdown)
// A2 190 = Cyrus (reprint)
static bool is_cyrus(const CardId& id)
{
    if (id.expansion == "A2" && id.number == 150) return true;
    return false;
}

// A1 223, A1 270 = Giovanni
// A4b 334, A4b 335 = Giovanni reprints
static bool is_giovanni(const CardId& id)
{
    if (id.expansion == "A1"  && id.number == 223) return true;
    if (id.expansion == "A1"  && id.number == 270) return true;
    if (id.expansion == "A4b" && id.number == 334) return true;
    if (id.expansion == "A4b" && id.number == 335) return true;
    return false;
}

// A1a 068, A1a 082 = Leaf
// A4b 346, A4b 347 = Leaf reprints
static bool is_leaf(const CardId& id)
{
    if (id.expansion == "A1a" && id.number == 68)  return true;
    if (id.expansion == "A1a" && id.number == 82)  return true;
    if (id.expansion == "A4b" && id.number == 346) return true;
    if (id.expansion == "A4b" && id.number == 347) return true;
    return false;
}

// A2 155, A2 195 = Mars
// A4b 344, A4b 345 = Mars reprints
static bool is_mars(const CardId& id)
{
    if (id.expansion == "A2"  && id.number == 155) return true;
    if (id.expansion == "A2"  && id.number == 195) return true;
    if (id.expansion == "A4b" && id.number == 344) return true;
    if (id.expansion == "A4b" && id.number == 345) return true;
    return false;
}

// B1 225, B1 270 = Copycat
static bool is_copycat(const CardId& id)
{
    if (id.expansion == "B1" && id.number == 225) return true;
    if (id.expansion == "B1" && id.number == 270) return true;
    return false;
}

// B1 226, B1 271 = Lisia
static bool is_lisia(const CardId& id)
{
    if (id.expansion == "B1" && id.number == 226) return true;
    if (id.expansion == "B1" && id.number == 271) return true;
    return false;
}

// A4b 373 = Professor's Research
// P-A 007 = Professor's Research (Promo-A)
static bool is_professors_research(const CardId& id)
{
    if (id.expansion == "A4b" && id.number == 373) return true;
    if (id.expansion == "P-A" && id.number == 7)   return true;
    return false;
}

// ---------------------------------------------------------------------------
// Per-card effect implementations
// ---------------------------------------------------------------------------

// Sabrina: opponent chooses which bench slot to move to Active.
// Sets PendingResponse so the game loop asks the opponent before continuing.
static void apply_sabrina_effect(GameState& gs, int player)
{
    const int opponent          = (player + 1) % 2;
    gs.pending_response         = PendingResponse::SabrinaChoice;
    gs.pending_response_player  = opponent;
    // The actual swap is deferred to apply_choose_bench_slot().
}

// Cyrus: active player already chose the target slot (action.slot_index).
// Execute the bench-swap immediately.
static void apply_cyrus_effect(GameState& gs, int player, int target_bench_slot)
{
    assert(target_bench_slot >= 1 && target_bench_slot <= 3 &&
           "apply_cyrus_effect: target_bench_slot must be 1-3");

    const int opponent    = (player + 1) % 2;
    auto& active_slot     = gs.players[opponent].pokemon_slots[0];
    auto& bench_slot      = gs.players[opponent].pokemon_slots[target_bench_slot];

    assert(active_slot.has_value() && "apply_cyrus_effect: opponent has no active Pokemon");
    assert(bench_slot.has_value()  && "apply_cyrus_effect: target bench slot is empty");

    // Clear volatile status on the current active (it moves to bench)
    active_slot->clear_volatile_status();

    std::swap(active_slot, bench_slot);
}

// Giovanni: all attacks this turn deal +10 damage.
// Increments the generic attack_boost field (additive with other sources).
static void apply_giovanni_effect(GameState& gs)
{
    gs.attack_boost += 10;
}

// Leaf: reduce the Active Pokemon's retreat cost by 2 this turn.
// Increments the generic retreat_reduction field (additive with other sources).
static void apply_leaf_effect(GameState& gs)
{
    gs.retreat_reduction += 2;
}

// Mars: opponent shuffles their hand into their deck, then draws
// (3 - opponent.points) cards — i.e. the number of points they still need to win.
static void apply_mars_effect(GameState& gs, int player, std::mt19937& rng)
{
    const int opponent = (player + 1) % 2;
    auto& opp_state    = gs.players[opponent];

    // Shuffle opponent's hand back into their deck
    for (Card& c : opp_state.hand)
        opp_state.deck.cards.push_back(std::move(c));
    opp_state.hand.clear();
    opp_state.deck.shuffle(rng);

    // Draw (3 - points) cards
    const int draw_count = std::max(0, 3 - opp_state.points);
    for (int i = 0; i < draw_count && !opp_state.deck.cards.empty(); ++i)
    {
        opp_state.hand.push_back(opp_state.deck.cards.front());
        opp_state.deck.cards.erase(opp_state.deck.cards.begin());
    }
}

// Copycat: record opponent's hand size, shuffle current player's hand into
// their deck, then draw that many cards.
static void apply_copycat_effect(GameState& gs, int player, std::mt19937& rng)
{
    const int opponent       = (player + 1) % 2;
    const int draw_count     = static_cast<int>(gs.players[opponent].hand.size());
    auto& ps                 = gs.players[player];

    // Shuffle current player's hand back into their deck
    // (the Copycat card itself was already removed by apply_action before this call)
    for (Card& c : ps.hand)
        ps.deck.cards.push_back(std::move(c));
    ps.hand.clear();
    ps.deck.shuffle(rng);

    // Draw equal to opponent's hand size at time of playing
    for (int i = 0; i < draw_count && !ps.deck.cards.empty(); ++i)
    {
        ps.hand.push_back(ps.deck.cards.front());
        ps.deck.cards.erase(ps.deck.cards.begin());
    }
}

// Lisia: put 2 random Basic Pokemon with HP <= 50 from deck into hand.
static void apply_lisia_effect(GameState& gs, int player, std::mt19937& rng)
{
    auto& ps = gs.players[player];

    // Collect indices of qualifying cards
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < ps.deck.cards.size(); ++i)
    {
        const Card& c = ps.deck.cards[i];
        if (c.type == CardType::Pokemon && c.stage == 0 && c.hp <= 50)
            candidates.push_back(i);
    }

    // Randomly pick up to 2 (without replacement), highest index first to keep erase safe
    const int picks = std::min(static_cast<int>(candidates.size()), 2);
    for (int n = 0; n < picks; ++n)
    {
        // Pick a random remaining candidate
        std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1 - n);
        // Shuffle the remaining candidates and take the first
        std::size_t pick_pos = dist(rng);
        std::swap(candidates[pick_pos], candidates[candidates.size() - 1 - n]);
    }

    // Sort the chosen indices descending so erasing doesn't shift earlier indices
    std::vector<std::size_t> chosen(candidates.end() - picks, candidates.end());
    std::sort(chosen.begin(), chosen.end(), std::greater<std::size_t>());

    for (std::size_t idx : chosen)
    {
        ps.hand.push_back(ps.deck.cards[idx]);
        ps.deck.cards.erase(ps.deck.cards.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    ps.deck.shuffle(rng);
}

// Professor's Research: draw 2 cards from the top of the deck.
static void apply_professors_research_effect(GameState& gs, int player)
{
    auto& ps = gs.players[player];
    for (int i = 0; i < 2 && !ps.deck.cards.empty(); ++i)
    {
        ps.hand.push_back(ps.deck.cards.front());
        ps.deck.cards.erase(ps.deck.cards.begin());
    }
}

// ---------------------------------------------------------------------------
// apply_supporter_effect
// ---------------------------------------------------------------------------

void apply_supporter_effect(GameState& gs, int player, const Action& action, std::mt19937& rng)
{
    if (is_sabrina(action.card_id))
    {
        apply_sabrina_effect(gs, player);
    }
    else if (is_cyrus(action.card_id))
    {
        apply_cyrus_effect(gs, player, action.slot_index);
    }
    else if (is_giovanni(action.card_id))
    {
        apply_giovanni_effect(gs);
    }
    else if (is_leaf(action.card_id))
    {
        apply_leaf_effect(gs);
    }
    else if (is_mars(action.card_id))
    {
        apply_mars_effect(gs, player, rng);
    }
    else if (is_copycat(action.card_id))
    {
        apply_copycat_effect(gs, player, rng);
    }
    else if (is_lisia(action.card_id))
    {
        apply_lisia_effect(gs, player, rng);
    }
    else if (is_professors_research(action.card_id))
    {
        apply_professors_research_effect(gs, player);
    }
    else
    {
        // Unimplemented supporter — card is already discarded; warn and continue.
        std::cerr << "[WARN] apply_supporter_effect: unimplemented supporter effect for "
                  << action.card_id.to_string() << "\n";
    }
}

// ---------------------------------------------------------------------------
// apply_choose_bench_slot  (opponent response to Sabrina)
// ---------------------------------------------------------------------------

void apply_choose_bench_slot(GameState& gs, const Action& action)
{
    assert(gs.pending_response == PendingResponse::SabrinaChoice &&
           "apply_choose_bench_slot: no pending SabrinaChoice response");
    assert(action.slot_index >= 1 && action.slot_index <= 3 &&
           "apply_choose_bench_slot: slot_index must be 1-3");

    const int responder = gs.pending_response_player;
    auto& active_slot   = gs.players[responder].pokemon_slots[0];
    auto& bench_slot    = gs.players[responder].pokemon_slots[action.slot_index];

    assert(active_slot.has_value() && "apply_choose_bench_slot: responder has no active Pokemon");
    assert(bench_slot.has_value()  && "apply_choose_bench_slot: target bench slot is empty");

    // Clear volatile status on the current active (it moves to bench)
    active_slot->clear_volatile_status();

    std::swap(active_slot, bench_slot);

    // Clear the pending response
    gs.pending_response        = PendingResponse::None;
    gs.pending_response_player = -1;
}

} // namespace ptcgp_sim
