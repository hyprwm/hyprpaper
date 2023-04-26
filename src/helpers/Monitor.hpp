#pragma once

#include "../defines.hpp"
#include "PoolBuffer.hpp"
#include "../render/LayerSurface.hpp"

struct SMonitor {
    std::string name = "";
    std::string description = "";
    wl_output* output = nullptr;
    uint32_t wayland_name = 0;
    Vector2D size;
    int scale;

    bool readyForLS = false;
    bool hasATarget = true;

    uint32_t configureSerial = 0;
    SPoolBuffer buffer;

    bool wantsReload = false;
    bool wantsACK = false;
    bool initialized = false;

    std::vector<std::unique_ptr<CLayerSurface>> layerSurfaces;
    CLayerSurface* pCurrentLayerSurface = nullptr;
};