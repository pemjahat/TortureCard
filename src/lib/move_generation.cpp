#include "ptcgp_sim/move_generation.h"
#include "ptcgp_sim/toolitem_effects.h"
#include <algorithm>
#include <map>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// energy_satisfies_cost
//
// Returns true if `available` covers `required`.
// Colorless in `required` matches any single energy in `available`.
// Typed requirements must be matched by the same type.
// ---------------------------------------------------------------------------
bool energy_satisfies_cost(const std::vector<EnergyType>& available,
                           const std::vector<EnergyType>& required)
{
    if (required.empty()) return true;

    // Build a mutable frequency map of available energy
    std::map<EnergyType, int> pool;
    for (EnergyType e : available)
        pool[e]++;

    // First pass: satisfy typed (non-Colorless) requirements
    for (EnergyType req : required)
    {
        if (req == EnergyType::Colorless) continue;
        auto it = pool.find(req);
        if (it == pool.end() || it->second == 0)
            return false;
        it->second--;
    }

    // Second pass: satisfy Colorless requirements with whatever remains
    int colorless_needed = static_cast<int>(
        std::count(required.begin(), required.end(), EnergyType::Colorless));

    int remaining_total = 0;
    for (auto& [type, cnt] : pool)
        remaining_total += cnt;

    return remaining_total >= colorless_needed;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool slot_has_tool(const std::array<std::optional<InPlayPokemon>, 4>& slots, int idx)
{
    return slots[idx].has_value() && slots[idx]->attached_tool.has_value();
}

// Sabrina card identity check (mirrors is_sabrina in effects.cpp)
static bool is_sabrina_card(const CardId& id)
{
    if (id.expansion == "A1" && id.number == 225) return true;
    if (id.expansion == "A1" && id.number == 272) return true;
    return false;
}

// Cyrus card identity check (mirrors is_cyrus in effects.cpp)
static bool is_cyrus_card(const CardId& id)
{
    if (id.expansion == "A2" && id.number == 150) return true;
    return false;
}

// Find a card in `hand` by CardId; returns nullptr if not found.
static const Card* find_in_hand(const std::vector<Card>& hand, const CardId& id)
{
    for (const Card& c : hand)
        if (c.id == id) return &c;
    return nullptr;
}

// ---------------------------------------------------------------------------
// generate_legal_moves
// ---------------------------------------------------------------------------
std::vector<Action> generate_legal_moves(const GameState& gs, int player)
{
    std::vector<Action> moves;
    const PlayerState&  ps    = gs.players[player];
    const auto&         slots = ps.pokemon_slots;
    const auto&         hand  = ps.hand;

    // -----------------------------------------------------------------------
    // SABRINA RESPONSE WINDOW
    // When pending_response == SabrinaChoice, only the designated responder
    // may act, and their only legal actions are ChooseBenchSlot options.
    // -----------------------------------------------------------------------
    if (gs.pending_response == PendingResponse::SabrinaChoice)
    {
        if (player == gs.pending_response_player)
        {
            // Emit one ChooseBenchSlot per occupied bench slot
            for (int s = 1; s <= 3; ++s)
            {
                if (slots[s].has_value())
                    moves.push_back(Action::choose_bench_slot(s));
            }
        }
        // Active player has no moves while opponent holds priority
        return moves;
    }

    // -----------------------------------------------------------------------
    // SETUP PHASE
    // -----------------------------------------------------------------------
    if (gs.turn_phase == TurnPhase::Setup)
    {
        bool active_empty = !slots[0].has_value();

        for (const Card& c : hand)
        {
            if (c.type != CardType::Pokemon) continue;
            if (c.stage != 0) continue; // only Basic Pokemon

            if (active_empty)
            {
                // Must fill active slot first
                moves.push_back(Action::play_pokemon(c.id, 0));
            }
            else
            {
                // Active is filled; offer empty bench slots
                for (int i = 1; i <= 3; ++i)
                {
                    if (!slots[i].has_value())
                        moves.push_back(Action::play_pokemon(c.id, i));
                }
            }
        }

        // Once active is filled the player may also pass (done placing bench)
        if (!active_empty)
            moves.push_back(Action::pass());

        return moves;
    }

    // -----------------------------------------------------------------------
    // ACTION PHASE (and any other non-Setup phase where player has choices)
    // -----------------------------------------------------------------------
    if (gs.turn_phase != TurnPhase::Action)
    {
        // In Draw / Attack / Cleanup phases the engine drives the flow;
        // the only player choice is Pass (end turn / acknowledge).
        moves.push_back(Action::pass());
        return moves;
    }

    // Pass is always available
    moves.push_back(Action::pass());

    // --- PlayPokemon (Basic only, into empty slots) ---
    for (const Card& c : hand)
    {
        if (c.type != CardType::Pokemon) continue;
        if (c.stage != 0) continue;

        for (int i = 0; i <= 3; ++i)
        {
            if (!slots[i].has_value())
                moves.push_back(Action::play_pokemon(c.id, i));
        }
    }

    // --- Evolve (Stage 1 / Stage 2 from hand onto matching in-play Pokemon) ---
    // Rules: not on the player's first turn (turn_number == 1 for player 0,
    //        turn_number == 2 for player 1), and target must not have been
    //        placed this turn.
    {
        // Determine if this is the current player's first turn.
        // Player 0 acts on odd turns (1, 3, 5...), player 1 on even turns (2, 4, 6...).
        // First turn for player 0 = turn_number 1; for player 1 = turn_number 2.
        bool is_first_turn = (player == 0)
                                 ? (gs.turn_number == 1)
                                 : (gs.turn_number == 2);

        if (!is_first_turn)
        {
            for (const Card& c : hand)
            {
                if (c.type != CardType::Pokemon) continue;
                if (c.stage == 0) continue; // only Stage 1 / Stage 2
                if (!c.evolves_from.has_value()) continue;

                for (int i = 0; i <= 3; ++i)
                {
                    if (!slots[i].has_value()) continue;
                    const InPlayPokemon& ip = *slots[i];
                    if (ip.played_this_turn) continue;
                    // Stage must increase by exactly 1
                    if (c.stage != ip.card.stage + 1) continue;
                    // evolves_from name must match
                    if (c.evolves_from.value() != ip.card.name) continue;

                    moves.push_back(Action::evolve(c.id, i));
                }
            }
        }
    }

    // --- AttachEnergy (once per turn, energy generation starts turn 2) ---
    if (!gs.energy_attached_this_turn && gs.current_energy.has_value())
    {
        EnergyType e = *gs.current_energy;
        for (int i = 0; i <= 3; ++i)
        {
            if (slots[i].has_value())
                moves.push_back(Action::attach_energy(e, i));
        }
    }

    // --- Trainer cards from hand ---
    for (const Card& c : hand)
    {
        if (c.type != CardType::Trainer) continue;

        switch (c.trainer_type)
        {
            case TrainerType::Supporter:
                if (!gs.supporter_played_this_turn)
                {
                    // Sabrina: legal only when opponent has at least one bench Pokemon.
                    // Emit a single PlaySupporter (no slot — opponent chooses later).
                    if (is_sabrina_card(c.id))
                    {
                        const int opp = (player + 1) % 2;
                        bool opp_has_bench = false;
                        for (int s = 1; s <= 3; ++s)
                            if (gs.players[opp].pokemon_slots[s].has_value())
                            { opp_has_bench = true; break; }
                        if (opp_has_bench)
                            moves.push_back(Action::play_supporter(c.id));
                    }
                    // Cyrus: legal only when opponent has at least one damaged bench Pokemon.
                    // Emit one PlaySupporter per damaged bench slot (active player chooses).
                    else if (is_cyrus_card(c.id))
                    {
                        const int opp = (player + 1) % 2;
                        for (int s = 1; s <= 3; ++s)
                        {
                            const auto& bench = gs.players[opp].pokemon_slots[s];
                            if (bench.has_value() && bench->damage_counters > 0)
                                moves.push_back(Action::play_supporter(c.id, s));
                        }
                    }
                    // Generic supporter: always legal (once per turn)
                    else
                    {
                        moves.push_back(Action::play_supporter(c.id));
                    }
                }
                break;

            case TrainerType::Item:
                // Potion targets a specific damaged Pokemon slot.
                if (is_potion_item(c.id))
                {
                    for (int i = 0; i <= 3; ++i)
                    {
                        if (slots[i].has_value() && slots[i]->damage_counters > 0)
                            moves.push_back(Action::play_item(c.id, i));
                    }
                }
                else
                {
                    moves.push_back(Action::play_item(c.id));
                }
                break;

            case TrainerType::Tool:
                // One Tool per Pokemon slot
                for (int i = 0; i <= 3; ++i)
                {
                    if (slots[i].has_value() && !slot_has_tool(slots, i))
                        moves.push_back(Action::play_tool(c.id, i));
                }
                break;

            case TrainerType::Stadium:
                // Always legal (replaces existing Stadium)
                moves.push_back(Action::play_stadium(c.id));
                break;
        }
    }

    // --- Retreat (once per turn, active must cover retreat cost) ---
    if (!gs.retreated_this_turn && slots[0].has_value())
    {
        const InPlayPokemon& active = *slots[0];
        // retreat_reduction (e.g. from Leaf) reduces the effective cost (minimum 0).
        const int full_cost      = static_cast<int>(active.card.retreat_cost.size());
        const int effective_cost = std::max(0, full_cost - gs.retreat_reduction);
        const std::vector<EnergyType> reduced_cost(
            active.card.retreat_cost.begin(),
            active.card.retreat_cost.begin() + effective_cost);
        if (energy_satisfies_cost(active.attached_energy, reduced_cost))
        {
            for (int i = 1; i <= 3; ++i)
            {
                if (slots[i].has_value())
                    moves.push_back(Action::retreat(i));
            }
        }
    }

    // --- Attack (active must have sufficient energy; once per turn) ---
    if (!gs.attacked_this_turn && slots[0].has_value())
    {
        const InPlayPokemon& active = *slots[0];
        for (int i = 0; i < static_cast<int>(active.card.attacks.size()); ++i)
        {
            const Attack& atk = active.card.attacks[i];
            if (energy_satisfies_cost(active.attached_energy, atk.energy_required))
                moves.push_back(Action::attack(i));
        }
    }

    return moves;
}

} // namespace ptcgp_sim
