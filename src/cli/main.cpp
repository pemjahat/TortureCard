#include "ptcgp_sim.h"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <algorithm>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void print_card(const ptcgp_sim::Card& c)
{
    std::cout << "  id          : " << c.id.to_string() << "  "
              << "(" << c.id.expansion << " / #" << c.id.number << ")\n"
              << "  name        : " << c.name << "\n"
              << "  hp          : " << c.hp   << "\n"
              << "  energy type : " << ptcgp_sim::energy_to_string(c.energy_type) << "\n";

    if (c.weakness)
        std::cout << "  weakness    : " << ptcgp_sim::energy_to_string(*c.weakness) << "\n";
    else
        std::cout << "  weakness    : none\n";

    std::cout << "  retreat cost: ";
    if (c.retreat_cost.empty()) 
    {
        std::cout << "free\n";
    } 
    else 
    {
        for (std::size_t i = 0; i < c.retreat_cost.size(); ++i) 
        {
            if (i) std::cout << ", ";
            std::cout << ptcgp_sim::energy_to_string(c.retreat_cost[i]);
        }
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------------------
// Subcommand: util
// ---------------------------------------------------------------------------

static int cmd_util(int argc, char* argv[]) 
{
    if (argc < 2) 
    {
    std::cerr << "util: no sub-option given\n"
                  << "  --fetch_card <ID>                         Print card details (e.g. \"A1 002\")\n"
                  << "  --validate_deck <deck.json>               Validate a deck JSON file\n"
                  << "  --simulate_turn <deck1.json> <deck2.json> Initialize a GameState and print summary\n"
                  << "  --dump_moves    <deck1.json> <deck2.json> Print game state and all legal moves\n"
                  << "  --build_dictionary                        (Re)build mechanic dictionary files\n";
        return 1;
    }

    std::string opt = argv[1];

    if (opt == "--build_dictionary")
    {
        bool ok = ptcgp_sim::Database::build_dictionaries();
        return ok ? 0 : 1;
    }

    if (opt == "--fetch_card") 
    {
        if (argc < 3) 
        {
            std::cerr << "util --fetch_card requires <ID>, e.g. \"A1 002\"\n";
            return 1;
        }
        std::string full_id = argv[2];

        // Parse the id string into a structured CardId
        auto space_pos = full_id.find(' ');
        assert(space_pos != std::string::npos && space_pos > 0 && space_pos + 1 < full_id.size()
            && "Invalid card id format. Expected \"<expansion> <number>\", e.g. \"A1 002\"");

        ptcgp_sim::CardId card_id = ptcgp_sim::Database::parse_id(full_id);

        std::cout << "Loading database...\n";
        ptcgp_sim::Database db = ptcgp_sim::Database::load();
        std::cout << "Loaded " << db.size() << " cards.\n\n";

        const ptcgp_sim::Card* card = db.find_by_id(card_id);
        if (!card) 
        {
            std::cerr << "Card not found: \"" << full_id << "\"\n";
            return 1;
        }
        print_card(*card);
        return 0;
    }

    if (opt == "--validate_deck")
    {
        if (argc < 3)
        {
            std::cerr << "util --validate_deck requires <deck.json>\n";
            return 1;
        }
        const std::string deck_path = argv[2];

        std::cout << "Loading database...\n";
        ptcgp_sim::Database db = ptcgp_sim::Database::load();
        std::cout << "Loaded " << db.size() << " cards.\n\n";

        ptcgp_sim::Deck deck = ptcgp_sim::Deck::load_from_json(deck_path, db);

        // Print summary
        std::cout << "Energy    : ";
        if (deck.energy_types.empty())
        {
            std::cout << "(none)";
        }
        else
        {
            for (std::size_t ei = 0; ei < deck.energy_types.size(); ++ei)
            {
                if (ei) std::cout << ", ";
                std::cout << ptcgp_sim::energy_to_string(deck.energy_types[ei]);
            }
        }
        std::cout << "\n";
        std::cout << "Total cards: " << deck.total_cards() << "\n\n";

        std::cout << "Cards:\n";
        for (const auto& entry : deck.entries)
        {
            const ptcgp_sim::Card* card = db.find_by_id(entry.id);
            std::string card_name = card ? card->name : "(unknown)";
            std::cout << "  x" << entry.count
                      << "  " << entry.id.to_string()
                      << "  " << card_name << "\n";
        }

        // Validate
        std::vector<std::string> errors;
        bool valid = deck.validate(errors);
        std::cout << "\nValidation: " << (valid ? "PASSED" : "FAILED") << "\n";
        if (!valid)
        {
            for (const auto& err : errors)
                std::cerr << "  [ERROR] " << err << "\n";
            return 1;
        }
        return 0;
    }

    if (opt == "--simulate_turn")
    {
        if (argc < 4)
        {
            std::cerr << "util --simulate_turn requires <deck1.json> <deck2.json>\n";
            return 1;
        }
        const std::string deck1_path = argv[2];
        const std::string deck2_path = argv[3];

        std::cout << "Loading database...\n";
        ptcgp_sim::Database db = ptcgp_sim::Database::load();
        std::cout << "Loaded " << db.size() << " cards.\n\n";

        ptcgp_sim::Deck deck0 = ptcgp_sim::Deck::load_from_json(deck1_path, db);
        ptcgp_sim::Deck deck1 = ptcgp_sim::Deck::load_from_json(deck2_path, db);

        // Validate both decks before proceeding
        std::vector<std::string> errors0, errors1;
        bool valid0 = deck0.validate(errors0);
        bool valid1 = deck1.validate(errors1);

        if (!valid0)
        {
            std::cerr << "Deck 1 validation FAILED:\n";
            for (const auto& e : errors0) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }
        if (!valid1)
        {
            std::cerr << "Deck 2 validation FAILED:\n";
            for (const auto& e : errors1) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }

        // Initialize game state and deal valid starting hands
        ptcgp_sim::GameState gs = ptcgp_sim::GameState::make(deck0, deck1);
        std::mt19937 rng(std::random_device{}());
        gs.deal_starting_hands(rng);

        // Print game state summary
        std::cout << "=== Game State Initialized ===\n";
        std::cout << "Turn number   : " << gs.turn_number << "\n";
        std::cout << "Turn phase    : " << ptcgp_sim::GameState::phase_name(gs.turn_phase) << "\n";
        std::cout << "Current player: " << gs.current_player << "\n";
        std::cout << "Game over     : " << (gs.game_over ? "yes" : "no") << "\n\n";

        for (int i = 0; i < 2; ++i)
        {
            const auto& p = gs.players[i];
            std::cout << "--- Player " << i << " ---\n";
            std::cout << "  Deck size      : " << p.deck.total_cards() << "\n";
            std::cout << "  Hand size      : " << p.hand.size() << "\n";
            std::cout << "  Discard pile   : " << p.discard_pile.size() << "\n";
            std::cout << "  Energy discard : " << p.energy_discard.size() << "\n";
            std::cout << "  Points         : " << p.points << "\n";
            std::cout << "  Active slot : " << (p.active().has_value() ? p.active()->card.name : "(empty)") << "\n";
            std::cout << "  Bench slots : ";
            int bench_count = 0;
            for (int s = 1; s <= 3; ++s)
                if (p.pokemon_slots[s].has_value()) ++bench_count;
            std::cout << bench_count << " / 3 occupied\n";
        }

        return 0;
    }

    if (opt == "--dump_moves")
    {
        if (argc < 4)
        {
            std::cerr << "util --dump_moves requires <deck1.json> <deck2.json>\n";
            return 1;
        }
        const std::string deck1_path = argv[2];
        const std::string deck2_path = argv[3];

        std::cout << "Loading database...\n";
        ptcgp_sim::Database db = ptcgp_sim::Database::load();
        std::cout << "Loaded " << db.size() << " cards.\n\n";

        ptcgp_sim::Deck deck0 = ptcgp_sim::Deck::load_from_json(deck1_path, db);
        ptcgp_sim::Deck deck1 = ptcgp_sim::Deck::load_from_json(deck2_path, db);

        std::vector<std::string> errors0, errors1;
        if (!deck0.validate(errors0))
        {
            std::cerr << "Deck 1 validation FAILED:\n";
            for (const auto& e : errors0) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }
        if (!deck1.validate(errors1))
        {
            std::cerr << "Deck 2 validation FAILED:\n";
            for (const auto& e : errors1) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }

        // Build a fresh game state in Setup phase and deal valid starting hands
        ptcgp_sim::GameState gs = ptcgp_sim::GameState::make(deck0, deck1);
        std::mt19937 rng(std::random_device{}());
        gs.deal_starting_hands(rng);

        // Helper: trainer type label
        auto trainer_type_str = [](ptcgp_sim::TrainerType tt) -> const char*
        {
            switch (tt)
            {
                case ptcgp_sim::TrainerType::Item:      return "Item";
                case ptcgp_sim::TrainerType::Tool:      return "Tool";
                case ptcgp_sim::TrainerType::Supporter: return "Supporter";
                case ptcgp_sim::TrainerType::Stadium:   return "Stadium";
                default:                                return "?";
            }
        };

        // Helper: print a single InPlayPokemon slot
        auto print_slot = [&](const std::optional<ptcgp_sim::InPlayPokemon>& slot,
                               const std::string& label)
        {
            if (!slot.has_value())
            {
                std::cout << "    " << label << ": (empty)\n";
                return;
            }
            const auto& ip = *slot;
            std::cout << "    " << label << ": " << ip.card.name
                      << " (" << ip.card.id.to_string() << ")"
                      << "  HP: " << ip.remaining_hp() << "/" << ip.card.hp
                      << "  Stage: " << ip.card.stage;
            if (!ip.attached_energy.empty())
            {
                std::cout << "  Energy: [";
                for (std::size_t ei = 0; ei < ip.attached_energy.size(); ++ei)
                {
                    if (ei) std::cout << ", ";
                    std::cout << ptcgp_sim::energy_to_string(ip.attached_energy[ei]);
                }
                std::cout << "]";
            }
            if (ip.attached_tool.has_value())
                std::cout << "  Tool: " << ip.attached_tool->name;
            std::cout << "\n";
        };

        // Print full game state
        std::cout << "=== Game State ===\n";
        std::cout << "Turn number   : " << gs.turn_number << "\n";
        std::cout << "Turn phase    : " << ptcgp_sim::GameState::phase_name(gs.turn_phase) << "\n";
        std::cout << "Current player: " << gs.current_player << "\n";
        if (gs.current_stadium.has_value())
            std::cout << "Stadium       : " << gs.current_stadium->to_string() << "\n";
        else
            std::cout << "Stadium       : (none)\n";
        std::cout << "\n";

        for (int p = 0; p < 2; ++p)
        {
            const auto& player = gs.players[p];
            std::cout << "--- Player " << p << " ---\n";
            std::cout << "  Points: " << player.points << "\n";
            // Energy discard bin
            std::cout << "  Energy discard: ";
            if (player.energy_discard.empty())
            {
                std::cout << "(none)\n";
            }
            else
            {
                std::cout << "[";
                for (std::size_t ei = 0; ei < player.energy_discard.size(); ++ei)
                {
                    if (ei) std::cout << ", ";
                    std::cout << ptcgp_sim::energy_to_string(player.energy_discard[ei]);
                }
                std::cout << "]\n";
            }
            std::cout << "  Hand (" << player.hand.size() << " cards):\n";
            for (const auto& c : player.hand)
            {
                if (c.type == ptcgp_sim::CardType::Pokemon)
                    std::cout << "    [Pokemon] " << c.name
                              << " (" << c.id.to_string() << ")  Stage: " << c.stage << "\n";
                else
                    std::cout << "    [Trainer/" << trainer_type_str(c.trainer_type) << "] "
                              << c.name << " (" << c.id.to_string() << ")\n";
            }
            std::cout << "  Pokemon slots:\n";
            print_slot(player.pokemon_slots[0], "Active");
            for (int s = 1; s <= 3; ++s)
            {
                std::string bench_label = "Bench " + std::to_string(s);
                print_slot(player.pokemon_slots[s], bench_label);
            }
            std::cout << "\n";
        }

        // Generate and print legal moves for current player
        int cur = gs.current_player;
        std::vector<ptcgp_sim::Action> moves = ptcgp_sim::generate_legal_moves(gs, cur);

        std::cout << "=== Legal Moves for Player " << cur << " ===\n";
        if (moves.empty())
        {
            std::cout << "No legal moves available\n";
        }
        else
        {
            for (std::size_t i = 0; i < moves.size(); ++i)
            {
                // Build human-readable description
                const auto& m = moves[i];
                std::cout << (i + 1) << ". ";
                switch (m.type)
                {
                    case ptcgp_sim::ActionType::PlayPokemon:
                    {
                        const ptcgp_sim::Card* c = db.find_by_id(m.card_id);
                        std::string name = c ? c->name : m.card_id.to_string();
                        std::cout << "PlayPokemon: " << name
                                  << " (" << m.card_id.to_string() << ")"
                                  << " -> slot " << m.slot_index << "\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::AttachEnergy:
                    {
                        const auto& slot = gs.players[cur].pokemon_slots[m.target_slot];
                        std::string pname = slot.has_value() ? slot->card.name : "?";
                        std::cout << "AttachEnergy: "
                                  << ptcgp_sim::energy_to_string(m.energy_type)
                                  << " -> " << pname << " (slot " << m.target_slot << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::Attack:
                    {
                        const auto& active = gs.players[cur].pokemon_slots[0];
                        std::string atk_name = "?";
                        if (active.has_value() &&
                            m.attack_index < static_cast<int>(active->card.attacks.size()))
                            atk_name = active->card.attacks[m.attack_index].name;
                        std::cout << "Attack: " << atk_name
                                  << " (index " << m.attack_index << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::Retreat:
                    {
                        const auto& bench = gs.players[cur].pokemon_slots[m.slot_index];
                        std::string bname = bench.has_value() ? bench->card.name : "?";
                        std::cout << "Retreat: -> " << bname
                                  << " (slot " << m.slot_index << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::PlaySupporter:
                    {
                        const ptcgp_sim::Card* c = db.find_by_id(m.card_id);
                        std::string name = c ? c->name : m.card_id.to_string();
                        std::cout << "PlaySupporter: " << name
                                  << " (" << m.card_id.to_string() << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::PlayItem:
                    {
                        const ptcgp_sim::Card* c = db.find_by_id(m.card_id);
                        std::string name = c ? c->name : m.card_id.to_string();
                        std::cout << "PlayItem: " << name
                                  << " (" << m.card_id.to_string() << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::PlayTool:
                    {
                        const ptcgp_sim::Card* c = db.find_by_id(m.card_id);
                        std::string name = c ? c->name : m.card_id.to_string();
                        const auto& slot = gs.players[cur].pokemon_slots[m.slot_index];
                        std::string pname = slot.has_value() ? slot->card.name : "?";
                        std::cout << "PlayTool: " << name
                                  << " (" << m.card_id.to_string() << ")"
                                  << " -> " << pname << " (slot " << m.slot_index << ")\n";
                        break;
                    }
                    case ptcgp_sim::ActionType::PlayStadium:
                    {
                        const ptcgp_sim::Card* c = db.find_by_id(m.card_id);
                        std::string name = c ? c->name : m.card_id.to_string();
                        std::cout << "PlayStadium: " << name
                                  << " (" << m.card_id.to_string() << ")\n";
                        break;
                    }
                    default:
                        std::cout << m.to_string() << "\n";
                        break;
                }
            }
        }
        return 0;
    }

    std::cerr << "util: unknown option \"" << opt << "\"\n";
    return 1;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) 
{
    std::cout << "ptcgp_sim CLI v0.1.0\n";

    if (argc < 2) 
    {
        std::cout << "Usage: ptcgp_cli <command> [options]\n"
                  << "Commands:\n"
                  << "  sim   <deck1.json> <deck2.json> [--games N]  Run simulation\n"
                  << "  util  --fetch_card <ID>                      Print card details\n"
                  << "  util  --validate_deck <deck.json>            Validate a deck\n"
                  << "  util  --simulate_turn <d1.json> <d2.json>    Initialize and print game state\n"
                  << "  util  --dump_moves    <d1.json> <d2.json>    Print game state + legal moves\n"
                  << "  util  --build_dictionary                     (Re)build mechanic dictionary files\n";
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "util") 
    {
        return cmd_util(argc - 1, argv + 1);
    }

    if (cmd == "sim") 
    {
        // Usage: ptcgp_cli sim <deck1.json> <deck2.json> [--verbose] [--seed <N>]
        if (argc < 4)
        {
            std::cerr << "sim: requires <deck1.json> <deck2.json>\n"
                      << "  Optional flags:\n"
                      << "    --verbose       Print turn-by-turn log\n"
                      << "    --seed <N>      Use a fixed RNG seed for reproducibility\n";
            return 1;
        }

        const std::string deck1_path = argv[2];
        const std::string deck2_path = argv[3];

        bool     verbose   = false;
        uint64_t seed      = std::random_device{}();
        bool     has_seed  = false;

        for (int i = 4; i < argc; ++i)
        {
            std::string flag = argv[i];
            if (flag == "--verbose")
            {
                verbose = true;
            }
            else if (flag == "--seed" && i + 1 < argc)
            {
                seed     = static_cast<uint64_t>(std::stoull(argv[++i]));
                has_seed = true;
            }
        }

        std::cout << "Loading database...\n";
        ptcgp_sim::Database db = ptcgp_sim::Database::load();
        std::cout << "Loaded " << db.size() << " cards.\n\n";

        ptcgp_sim::Deck deck0 = ptcgp_sim::Deck::load_from_json(deck1_path, db);
        ptcgp_sim::Deck deck1 = ptcgp_sim::Deck::load_from_json(deck2_path, db);

        // Validate both decks
        std::vector<std::string> errors0, errors1;
        bool valid0 = deck0.validate(errors0);
        bool valid1 = deck1.validate(errors1);

        if (!valid0)
        {
            std::cerr << "Deck 1 validation FAILED:\n";
            for (const auto& e : errors0) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }
        if (!valid1)
        {
            std::cerr << "Deck 2 validation FAILED:\n";
            for (const auto& e : errors1) std::cerr << "  [ERROR] " << e << "\n";
            return 1;
        }

        std::cout << "Deck 1 energy: ";
        for (std::size_t i = 0; i < deck0.energy_types.size(); ++i)
        {
            if (i) std::cout << ", ";
            std::cout << ptcgp_sim::energy_to_string(deck0.energy_types[i]);
        }
        std::cout << "\n";

        std::cout << "Deck 2 energy: ";
        for (std::size_t i = 0; i < deck1.energy_types.size(); ++i)
        {
            if (i) std::cout << ", ";
            std::cout << ptcgp_sim::energy_to_string(deck1.energy_types[i]);
        }
        std::cout << "\n";

        if (has_seed)
            std::cout << "RNG seed: " << seed << "\n";
        std::cout << "\n";

        // Run the game
        std::mt19937 rng(static_cast<uint32_t>(seed));
        ptcgp_sim::AttachAttackPlayer player0;
        ptcgp_sim::AttachAttackPlayer player1;
        ptcgp_sim::GameLoop loop(&player0, &player1, rng, verbose);

        ptcgp_sim::GameState gs = ptcgp_sim::GameState::make(deck0, deck1);
        ptcgp_sim::SimulationResult result = loop.run(gs);

        // Print result
        std::cout << "\n=== Game Over ===\n";
        std::cout << "Turns played : " << result.turns << "\n";
        std::cout << "Player 0 pts : " << gs.players[0].points << "\n";
        std::cout << "Player 1 pts : " << gs.players[1].points << "\n";

        if (result.winner == 0)
            std::cout << "Winner       : Player 0\n";
        else if (result.winner == 1)
            std::cout << "Winner       : Player 1\n";
        else
            std::cout << "Result       : Draw (turn limit reached)\n";

        return 0;
    }

    std::cerr << "Unknown command: \"" << cmd << "\"\n";
    return 1;
}
