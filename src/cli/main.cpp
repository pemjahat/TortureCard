#include "ptcgp_sim.h"
#include <cassert>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* energy_type_name(ptcgp_sim::EnergyType e) 
{
    using E = ptcgp_sim::EnergyType;
    switch (e) {
        case E::Grass:     return "Grass";
        case E::Fire:      return "Fire";
        case E::Water:     return "Water";
        case E::Lightning: return "Lightning";
        case E::Psychic:   return "Psychic";
        case E::Fighting:  return "Fighting";
        case E::Darkness:  return "Darkness";
        case E::Metal:     return "Metal";
        case E::Dragon:    return "Dragon";
        case E::Colorless: return "Colorless";
        default:           return "Unknown";
    }
}

static void print_card(const ptcgp_sim::Card& c) 
{
    std::cout << "  id          : " << c.id.to_string() << "  "
              << "(" << c.id.expansion << " / #" << c.id.number << ")\n"
              << "  name        : " << c.name << "\n"
              << "  hp          : " << c.hp   << "\n"
              << "  energy type : " << energy_type_name(c.energy_type) << "\n";

    if (c.weakness)
        std::cout << "  weakness    : " << energy_type_name(*c.weakness) << "\n";
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
            std::cout << energy_type_name(c.retreat_cost[i]);
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
                  << "  --fetch_card <ID>   Print card details (e.g. \"A1 002\")\n";
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
                  << "  util  --fetch_card <ID>                     Print card details\n";
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
