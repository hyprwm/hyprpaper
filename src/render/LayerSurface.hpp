#pragma once

#include "../defines.hpp"

struct SMonitor;

class CLayerSurface {
public:
    CLayerSurface(SMonitor*);
    ~CLayerSurface();

    SMonitor* m_pMonitor = nullptr;

    zwlr_layer_surface_v1* pLayerSurface = nullptr;
    wl_surface* pSurface = nullptr;

    bool m_bCurrent = false;
};