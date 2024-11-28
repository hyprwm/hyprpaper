#pragma once

#include "../defines.hpp"
#include <hyprgraphics/cairo/CairoSurface.hpp>

class CWallpaperTarget {
  public:
    ~CWallpaperTarget();

    void                            create(const std::string& path);

    std::string                     m_szPath;

    Vector2D                        m_vSize;

    bool                            m_bHasAlpha = true;

    SP<Hyprgraphics::CCairoSurface> m_pCairoSurface;
};
