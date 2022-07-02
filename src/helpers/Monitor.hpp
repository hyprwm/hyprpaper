#pragma once

#include "../defines.hpp"
#include "PoolBuffer.hpp"

struct SMonitor {
    std::string name = "";
    wl_output* output = nullptr;
    uint32_t wayland_name = 0;
    Vector2D size;
    int scale;

    bool readyForLS = false;
    bool hasATarget = true;

    zwlr_layer_surface_v1* pLayerSurface = nullptr;
    wl_surface* pSurface = nullptr;
    uint32_t configureSerial = 0;
    SPoolBuffer buffer;

    bool wantsReload = false;
    bool wantsACK = false;
    bool initialized = false;
};