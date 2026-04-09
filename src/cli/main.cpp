#include "ptcgp_sim.h"
#include <cassert>
#include <iostream>
#include <string>

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
                  << "  --fetch_card <ID>           Print card details (e.g. \"A1 002\")\n"
                  << "  --validate_deck <deck.json>  Validate a deck JSON file\n";
        return 1;
    }

    std::string opt = argv[1];

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
                  << "  util  --validate_deck <deck.json>            Validate a deck\n";
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "util") 
    {
        return cmd_util(argc - 1, argv + 1);
    }

    if (cmd == "sim") 
    {
        ptcgp_sim::Simulator sim;
        (void)sim;
        std::cout << "sim: not yet implemented\n";
        return 0;
    }

    std::cerr << "Unknown command: \"" << cmd << "\"\n";
    return 1;
}
