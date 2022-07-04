#pragma once

#include "../defines.hpp"
#include "../helpers/Jpeg.hpp"

class CWallpaperTarget {
public:
    
    void        create(const std::string& path);
    void        render();

    std::string m_szPath;

    Vector2D    m_vSize;

    bool        m_bHasAlpha = true;

    cairo_surface_t* m_pCairoSurface;
};