#pragma once

#include "Memory.hpp"

struct SGlobalState {
    bool verbose = false;
};

inline UP<SGlobalState> g_state = makeUnique<SGlobalState>();