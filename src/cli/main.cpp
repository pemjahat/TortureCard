#include "ptcgp_sim.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "ptcgp_sim CLI v0.1.0\n";

    if (argc < 2) {
        std::cout << "Usage: ptcgp_cli <command> [options]\n"
                  << "Commands:\n"
                  << "  sim   <deck1.json> <deck2.json> [--games N]  Run simulation\n";
        return 0;
    }

    // TODO: parse argv and dispatch to ptcgp_sim::Simulator
    (void)argv;
    ptcgp_sim::Simulator sim;
    (void)sim;

    return 0;
}
