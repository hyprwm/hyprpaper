#pragma once

#include "../defines.hpp"
#include "protocols/fractional-scale-v1.hpp"
#include "protocols/viewporter.hpp"
#include "protocols/wayland.hpp"
#include "protocols/wlr-layer-shell-unstable-v1.hpp"

struct SMonitor;

class CLayerSurface {
  public:
    explicit CLayerSurface(SMonitor*);
    ~CLayerSurface();

    SMonitor*                 m_pMonitor = nullptr;

    SP<CCZwlrLayerSurfaceV1>  pLayerSurface        = nullptr;
    SP<CCWlSurface>           pSurface             = nullptr;
    SP<CCWpFractionalScaleV1> pFractionalScaleInfo = nullptr;
    SP<CCWpViewport>          pViewport            = nullptr;
    double                    fScale               = 1.0;
};
