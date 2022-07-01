#pragma once

#include "../defines.hpp"

class CWallpaperTarget {
public:
    
    void        create(const std::string& path);
    void        render();

    std::string m_szPath;

    Vector2D m_vSize;

    cairo_surface_t* m_pCairoSurface;
};