#pragma once

#include "../defines.hpp"

class CCZwlrLayerSurfaceV1;
class CCWlSurface;
class CCWpFractionalScaleV1;
class CCWpViewport;

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
