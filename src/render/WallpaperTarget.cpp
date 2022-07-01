#include "WallpaperTarget.hpp"

void CWallpaperTarget::create(const std::string& path) {
    m_szPath = path;

    cairo_surface_t* CAIROSURFACE = nullptr;
    if (path.find(".png") == path.length() - 4) {
        CAIROSURFACE = cairo_image_surface_create_from_png(path.c_str());
    } else if (path.find(".jpg") == path.length() - 4 || path.find(".jpeg") == path.length() - 5) {
        Debug::log(ERR, ".jpg images are not yet supported! :(");
        exit(1);
        return;
    } else {
        Debug::log(CRIT, "unrecognized image %s", path.c_str());
        exit(1);
        return;
    }

    if (cairo_surface_status(CAIROSURFACE) != CAIRO_STATUS_SUCCESS) {
        Debug::log(CRIT, "Failed to read image %s because of:\n%s", path.c_str(), cairo_status_to_string(cairo_surface_status(CAIROSURFACE)));
        exit(1);
    }

    m_pCairoSurface = CAIROSURFACE;
}