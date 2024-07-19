#pragma once

#include "../defines.hpp"
#include "../render/LayerSurface.hpp"

class CCWlOutput;

struct SMonitor {
    std::string                    name        = "";
    std::string                    description = "";
    SP<CCWlOutput>                 output;
    uint32_t                       wayland_name = 0;
    Vector2D                       size;
    int                            scale;

    bool                           readyForLS = false;
    bool                           hasATarget = true;

    bool                           wildcard = true;

    uint32_t                       configureSerial = 0;

    bool                           wantsReload = false;
    bool                           wantsACK    = false;
    bool                           initialized = false;

    std::vector<SP<CLayerSurface>> layerSurfaces;
    SP<CLayerSurface>              pCurrentLayerSurface = nullptr;

    void                           registerListeners();
};