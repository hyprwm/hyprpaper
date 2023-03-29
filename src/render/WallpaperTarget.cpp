#include "WallpaperTarget.hpp"

CWallpaperTarget::~CWallpaperTarget() {
    cairo_surface_destroy(m_pCairoSurface);
}

void CWallpaperTarget::create(const std::string& path) {
    m_szPath = path;

    const auto BEGINLOAD = std::chrono::system_clock::now();

    cairo_surface_t* CAIROSURFACE = nullptr;
    const auto len = path.length()+36;
    char cmd[len];
    sprintf(cmd, "file -b --mime-type %s", path.c_str());
    FILE *file = popen(cmd, "r");
    char file_type[16];
    fread(file_type, 1, 15, file);
    file_type[15]='\0';
    pclose(file);
    Debug::log(LOG, "File: %s", file_type);
    if (strncmp(file_type, "image/png", 9) == 0) { // file_type may have new line and EOF at the end
        CAIROSURFACE = cairo_image_surface_create_from_png(path.c_str());
    } else if (strncmp(file_type, "image/jpeg", 10) == 0) {
        CAIROSURFACE = JPEG::createSurfaceFromJPEG(path);
        m_bHasAlpha = false;
    } else {
        Debug::log(CRIT, "unrecognized image %s", path.c_str());
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
