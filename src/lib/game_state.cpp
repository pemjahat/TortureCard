#include "ptcgp_sim/game_state.h"

// GameState, PlayerState, and InPlayPokemon are fully defined as inline
// structs/methods in game_state.h.  This translation unit exists to satisfy
// the CMake GLOB_RECURSE that collects all src/lib/*.cpp files into the
// static library.  Any non-inline helper implementations can be added here.

namespace ptcgp_sim {
// (no out-of-line definitions required at this time)
} // namespace ptcgp_sim
