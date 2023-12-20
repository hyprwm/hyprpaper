#pragma once

#include "../defines.hpp"
#include "../helpers/Jpeg.hpp"
#include "../helpers/Webp.hpp"

class CWallpaperTarget {
public:

    ~CWallpaperTarget();

    void        create(const std::string& path);

    std::string m_szPath;

    Vector2D    m_vSize;

    bool        m_bHasAlpha = true;

    cairo_surface_t* m_pCairoSurface;
};
