#include "WallpaperTarget.hpp"

#include <chrono>
#include <magic.h>
#include "Egl.hpp"
#include "Renderer.hpp"

CWallpaperTarget::~CWallpaperTarget() {
    if (g_pEGL)
        g_pEGL->makeCurrent(EGL_NO_SURFACE);
    if (cpu.cairoSurface)
        cairo_surface_destroy(cpu.cairoSurface);
    if (gpu.textureID)
        glDeleteTextures(1, &gpu.textureID);
}

void CWallpaperTarget::create(const std::string& path) {
    m_szPath = path;

    const auto       BEGINLOAD = std::chrono::system_clock::now();

    cairo_surface_t* CAIROSURFACE = nullptr;
    const auto       len          = path.length();
    if (path.find(".png") == len - 4 || path.find(".PNG") == len - 4) {
        CAIROSURFACE = cairo_image_surface_create_from_png(path.c_str());
    } else if (path.find(".jpg") == len - 4 || path.find(".JPG") == len - 4 || path.find(".jpeg") == len - 5 || path.find(".JPEG") == len - 5) {
        CAIROSURFACE = JPEG::createSurfaceFromJPEG(path);
        m_bHasAlpha  = false;
    } else if (path.find(".bmp") == len - 4 || path.find(".BMP") == len - 4) {
        CAIROSURFACE = BMP::createSurfaceFromBMP(path);
        m_bHasAlpha  = false;
    } else if (path.find(".webp") == len - 5 || path.find(".WEBP") == len - 5) {
        CAIROSURFACE = WEBP::createSurfaceFromWEBP(path);
    } else {
        // magic is slow, so only use it when no recognized extension is found
        auto handle = magic_open(MAGIC_NONE | MAGIC_COMPRESS);
        magic_load(handle, nullptr);

        const auto type_str   = std::string(magic_file(handle, path.c_str()));
        const auto first_word = type_str.substr(0, type_str.find(" "));

        if (first_word == "PNG") {
            CAIROSURFACE = cairo_image_surface_create_from_png(path.c_str());
        } else if (first_word == "JPEG") {
            CAIROSURFACE = JPEG::createSurfaceFromJPEG(path);
            m_bHasAlpha  = false;
        } else if (first_word == "BMP") {
            CAIROSURFACE = BMP::createSurfaceFromBMP(path);
            m_bHasAlpha  = false;
        } else {
            Debug::log(CRIT, "unrecognized image %s", path.c_str());
            exit(1);
        }
    }

    if (cairo_surface_status(CAIROSURFACE) != CAIRO_STATUS_SUCCESS) {
        Debug::log(CRIT, "Failed to read image %s because of:\n%s", path.c_str(), cairo_status_to_string(cairo_surface_status(CAIROSURFACE)));
        exit(1);
    }

    m_vSize = {cairo_image_surface_get_width(CAIROSURFACE), cairo_image_surface_get_height(CAIROSURFACE)};

    const auto MS = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - BEGINLOAD).count() / 1000.f;

    Debug::log(LOG, "Preloaded target %s in %.2fms -> Pixel size: [%i, %i]", path.c_str(), MS, (int)m_vSize.x, (int)m_vSize.y);

    if (!g_pEGL) {
        cpu.cairoSurface = CAIROSURFACE;
        return;
    }

    Debug::log(LOG, "GPU mode, uploading the preloaded image into VRAM and deleting from RAM");

    g_pEGL->makeCurrent(EGL_NO_SURFACE);

    auto tex = g_pRenderer->glTex(cairo_image_surface_get_data(CAIROSURFACE), m_vSize);

    gpu.textureID = tex.texid;

    cairo_surface_destroy(CAIROSURFACE);
}
