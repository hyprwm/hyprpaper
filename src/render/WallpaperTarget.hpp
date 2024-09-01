#pragma once

#include "../defines.hpp"
#include "../helpers/Jpeg.hpp"
#include "../helpers/Bmp.hpp"
#include "../helpers/Webp.hpp"

class CWallpaperTarget {
  public:
    ~CWallpaperTarget();

    void        create(const std::string& path);

    std::string m_szPath;

    Vector2D    m_vSize;

    bool        m_bHasAlpha = true;

    struct {
        cairo_surface_t* cairoSurface = nullptr;
    } cpu;

    struct {
        uint32_t textureID = 0;
    } gpu;
};
