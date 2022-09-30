#pragma once

#include "../defines.hpp"

struct SMonitor;

class CLayerSurface {
public:
    explicit CLayerSurface(SMonitor*);
    ~CLayerSurface();

    SMonitor* m_pMonitor = nullptr;

    zwlr_layer_surface_v1* pLayerSurface = nullptr;
    wl_surface* pSurface = nullptr;
};
