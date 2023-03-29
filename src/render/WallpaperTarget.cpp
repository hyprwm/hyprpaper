#include "WallpaperTarget.hpp"

CWallpaperTarget::~CWallpaperTarget() {
    cairo_surface_destroy(m_pCairoSurface);
}

void CWallpaperTarget::create(const std::string& path) {
    m_szPath = path;

    const auto BEGINLOAD = std::chrono::system_clock::now();

    cairo_surface_t* CAIROSURFACE = nullptr;
    const std::string cmd = "file -b --mime-type " + path;
    std::string file_type(execAndGet(cmd.c_str()));
    if (file_type.contains("image/png")) { //Do not change to == as string has newline at the end
        CAIROSURFACE = cairo_image_surface_create_from_png(path.c_str());
    } else if (file_type.contains("image/jpeg")) { 
        CAIROSURFACE = JPEG::createSurfaceFromJPEG(path);
        m_bHasAlpha = false;
    }
    else if (file_type == "")
    {
        Debug::log(CRIT, "file command didn't return anything, are you sure file is installed?");
        exit(1);
    }
    else {
        Debug::log(CRIT, "unrecognized image %s", path.c_str());
        Debug::log(LOG, "Returned file type is: %s", file_type.c_str());
        exit(1);
    }

    if (cairo_surface_status(CAIROSURFACE) != CAIRO_STATUS_SUCCESS) {
        Debug::log(CRIT, "Failed to read image %s because of:\n%s", path.c_str(), cairo_status_to_string(cairo_surface_status(CAIROSURFACE)));
        exit(1);
    }

    m_vSize = { cairo_image_surface_get_width(CAIROSURFACE), cairo_image_surface_get_height(CAIROSURFACE) };

    const auto MS = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - BEGINLOAD).count() / 1000.f;

    Debug::log(LOG, "Preloaded target %s in %.2fms -> Pixel size: [%i, %i]", path.c_str(), MS, (int)m_vSize.x, (int)m_vSize.y);

    m_pCairoSurface = CAIROSURFACE;
}
