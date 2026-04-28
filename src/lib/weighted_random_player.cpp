#include "ptcgp_sim/weighted_random_player.h"

#include <cassert>
#include <numeric>
#include <random>
#include <stdexcept>

namespace ptcgp_sim
{

WeightedRandomPlayer::WeightedRandomPlayer()
    : rng_(std::random_device{}())
{}

WeightedRandomPlayer::WeightedRandomPlayer(uint32_t seed)
    : rng_(seed)
{}

// ---------------------------------------------------------------------------
// WeightedRandomPlayer::action_weight
// ---------------------------------------------------------------------------

int WeightedRandomPlayer::action_weight(ActionType type)
{
    switch (type)
    {
        case ActionType::PlayPokemon:   return 5;  // deckgym-core Place
        case ActionType::AttachEnergy:  return 10; // deckgym-core Attach
        case ActionType::Attack:        return 10; // deckgym-core Attack
        case ActionType::Retreat:       return 2;  // deckgym-core Retreat
        case ActionType::Pass:          return 1;  // deckgym-core EndTurn
        case ActionType::PlaySupporter: return 5;  // deckgym-core Play
        case ActionType::PlayItem:      return 5;  // deckgym-core Play
        case ActionType::PlayTool:      return 10; // deckgym-core AttachTool
        case ActionType::PlayStadium:   return 5;  // deckgym-core Play / UseStadium
        case ActionType::Evolve:        return 10; // deckgym-core Evolve
        case ActionType::UseAbility:    return 10; // deckgym-core UseAbility
    }

    // Reference-only deckgym-core actions intentionally not mapped here include:
    // DrawCard (handled by the game loop), MoveEnergy, UseCopiedAttack,
    // ApplyDamage (resolved internally by effects/attacks),
    // ScheduleDelayedSpotDamage, Heal, HealAndDiscardEnergy, MoveAllDamage,
    // Activate, CommunicatePokemon, ShufflePokemonIntoDeck,
    // ShuffleOwnCardsIntoDeck, ShuffleOpponentSupporter,
    // DiscardOpponentSupporter, DiscardOwnCards, AttachFromDiscard,
    // ApplyEeveeBagDamageBoost, HealAllEeveeEvolutions, DiscardFossil,
    // ReturnPokemonToHand, UseStadium, and Noop.
    assert(false && "WeightedRandomPlayer: unmapped ActionType");
    throw std::logic_error("WeightedRandomPlayer: unmapped ActionType");
}

// ---------------------------------------------------------------------------
// WeightedRandomPlayer::decide
// ---------------------------------------------------------------------------

Action WeightedRandomPlayer::decide(const GameState&,
                                    const std::vector<Action>& legal_moves)
{
    assert(!legal_moves.empty() && "WeightedRandomPlayer: no legal moves");
    if (legal_moves.empty())
        throw std::invalid_argument("WeightedRandomPlayer: no legal moves");

    std::vector<int> weights;
    weights.reserve(legal_moves.size());

    for (const Action& action : legal_moves)
    {
        const int weight = action_weight(action.type);
        assert(weight > 0 && "WeightedRandomPlayer: action weight must be positive");
        if (weight <= 0)
            throw std::logic_error("WeightedRandomPlayer: action weight must be positive");
        weights.push_back(weight);
    }

    const int total_weight = std::accumulate(weights.begin(), weights.end(), 0);
    assert(total_weight > 0 && "WeightedRandomPlayer: total weight must be positive");
    if (total_weight <= 0)
        throw std::logic_error("WeightedRandomPlayer: total weight must be positive");

    std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
    return legal_moves[dist(rng_)];
}

} // namespace ptcgp_sim
