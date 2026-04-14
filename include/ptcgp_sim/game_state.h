#pragma once

#include "card.h"
#include "deck.h"
#include <array>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace ptcgp_sim
{

// ---------------------------------------------------------------------------
// Status conditions a Pokemon can have (only one at a time)
// ---------------------------------------------------------------------------
enum class StatusCondition
{
    None,
    Poisoned,
    Asleep,
    Paralyzed,
    Burned,
    Confused,
};

// ---------------------------------------------------------------------------
// Turn phases within a single player's turn
// ---------------------------------------------------------------------------
enum class TurnPhase
{
    Setup,    // Initial placement phase (before turn 1)
    Draw,     // Draw a card from deck
    Action,   // Play cards, attach energy, use abilities
    Attack,   // Declare and resolve an attack
    Cleanup,  // End-of-turn effects, status damage, etc.
};

// ---------------------------------------------------------------------------
// A Pokemon card placed on the mat with its runtime battle state
// ---------------------------------------------------------------------------
struct InPlayPokemon
{
    Card            card;                          // Base card data (name, hp, type, etc.)
    int             damage_counters{0};            // Accumulated damage (not subtracted from hp)
    std::vector<EnergyType> attached_energy;       // Energy cards attached to this Pokemon
    StatusCondition status{StatusCondition::None}; // Current status condition
    bool            played_this_turn{true};        // True if placed on mat this turn
    std::optional<Card> attached_tool{};           // Tool card attached to this Pokemon (if any)

    // Convenience: remaining HP = base hp - damage_counters
    int remaining_hp() const
    {
        int rem = card.hp - damage_counters;
        return rem < 0 ? 0 : rem;
    }

    // True when damage_counters >= card's hp
    bool is_knocked_out() const
    {
        return damage_counters >= card.hp;
    }

    // Clear volatile status conditions (called when moving to bench or retreating)
    void clear_volatile_status()
    {
        if (status == StatusCondition::Paralyzed ||
            status == StatusCondition::Confused  ||
            status == StatusCondition::Asleep    ||
            status == StatusCondition::Burned)
        {
            status = StatusCondition::None;
        }
        // Poisoned persists on bench in some rule sets; kept here for flexibility
    }
};

// ---------------------------------------------------------------------------
// Per-player state
// ---------------------------------------------------------------------------
struct PlayerState
{
    Deck                                  deck;
    std::vector<Card>                     hand;
    std::vector<Card>                     discard_pile;
    int                                   points{0};  // Prize points scored (0-3)

    // Slot 0 = Active Pokemon, slots 1-3 = Bench
    std::array<std::optional<InPlayPokemon>, 4> pokemon_slots;

    // Convenience accessors
    std::optional<InPlayPokemon>&       active()       { return pokemon_slots[0]; }
    const std::optional<InPlayPokemon>& active() const { return pokemon_slots[0]; }
};

// ---------------------------------------------------------------------------
// Complete game state snapshot
// ---------------------------------------------------------------------------
struct GameState
{
    std::array<PlayerState, 2> players;

    int       turn_number{1};
    TurnPhase turn_phase{TurnPhase::Setup};
    int       current_player{0};  // 0 or 1
    bool      game_over{false};
    int       winner{-1};         // -1 = none, 0 = player 0, 1 = player 1

    // Per-turn flags (reset at the start of each new turn)
    bool energy_attached_this_turn{false};
    bool supporter_played_this_turn{false};
    bool retreated_this_turn{false};
    bool attacked_this_turn{false};

    // Stadium currently in play (nullopt if none)
    std::optional<CardId> current_stadium{};

    // Energy available to attach this turn (nullopt if not yet generated / turn 1)
    std::optional<EnergyType> current_energy{};

    // Initialize a fresh game from two decks
    static GameState make(const Deck& deck_p0, const Deck& deck_p1)
    {
        GameState gs;
        gs.players[0].deck = deck_p0;
        gs.players[1].deck = deck_p1;
        gs.turn_number     = 1;
        gs.turn_phase      = TurnPhase::Setup;
        gs.current_player  = 0;
        gs.game_over       = false;
        gs.winner          = -1;
        return gs;
    }

    // Advance to the next turn phase following: Draw->Action->Attack->Cleanup->Draw
    void advance_phase()
    {
        switch (turn_phase)
        {
            case TurnPhase::Setup:   turn_phase = TurnPhase::Draw;    break;
            case TurnPhase::Draw:    turn_phase = TurnPhase::Action;  break;
            case TurnPhase::Action:  turn_phase = TurnPhase::Attack;  break;
            case TurnPhase::Attack:  turn_phase = TurnPhase::Cleanup; break;
            case TurnPhase::Cleanup:
                // End of turn: switch player, increment turn number, reset flags, go to Draw
                current_player = (current_player + 1) % 2;
                turn_number   += 1;
                turn_phase     = TurnPhase::Draw;
                reset_turn_flags();
                break;
        }
    }

    // Reset all per-turn flags (called at the start of each new turn)
    void reset_turn_flags()
    {
        energy_attached_this_turn  = false;
        supporter_played_this_turn = false;
        retreated_this_turn        = false;
        attacked_this_turn         = false;
        current_energy             = std::nullopt;
    }

    // Check and set game_over / winner if a player has reached 3 points
    void check_win_condition()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (players[i].points >= 3)
            {
                game_over = true;
                winner    = i;
                return;
            }
        }
    }

    // Return a human-readable name for the current turn phase
    static const char* phase_name(TurnPhase p)
    {
        switch (p)
        {
            case TurnPhase::Setup:   return "Setup";
            case TurnPhase::Draw:    return "Draw";
            case TurnPhase::Action:  return "Action";
            case TurnPhase::Attack:  return "Attack";
            case TurnPhase::Cleanup: return "Cleanup";
            default:                 return "Unknown";
        }
    }

    // Shuffle both players' decks and deal each player a valid 5-card starting
    // hand (at least one stage-0 Pokemon) using Deck::deal_starting_hand().
    // Asserts if either player already has cards in hand (prevents double-dealing).
    void deal_starting_hands(std::mt19937& rng);
};

} // namespace ptcgp_sim
