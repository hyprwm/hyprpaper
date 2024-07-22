#include "Buffer.hpp"
#include "../Hyprpaper.hpp"
#include <gbm.h>
#include <xf86drm.h>
#include <drm_fourcc.h>
#include "Renderer.hpp"
#include "Egl.hpp"

#include "protocols/wayland.hpp"
#include "protocols/linux-dmabuf-v1.hpp"

static bool setCloexec(const int& FD) {
    long flags = fcntl(FD, F_GETFD);
    if (flags == -1) {
        return false;
    }

    if (fcntl(FD, F_SETFD, flags | FD_CLOEXEC) == -1) {
        return false;
    }

    return true;
}

static int createPoolFile(size_t size, std::string& name) {
    const auto XDGRUNTIMEDIR = getenv("XDG_RUNTIME_DIR");
    if (!XDGRUNTIMEDIR) {
        Debug::log(CRIT, "XDG_RUNTIME_DIR not set!");
        exit(1);
    }

    name = std::string(XDGRUNTIMEDIR) + "/.hyprpaper_XXXXXX";

    const auto FD = mkstemp((char*)name.c_str());
    if (FD < 0) {
        Debug::log(CRIT, "createPoolFile: fd < 0");
        exit(1);
    }

    if (!setCloexec(FD)) {
        close(FD);
        Debug::log(CRIT, "createPoolFile: !setCloexec");
        exit(1);
    }

    if (ftruncate(FD, size) < 0) {
        close(FD);
        Debug::log(CRIT, "createPoolFile: ftruncate < 0");
        exit(1);
    }

    return FD;
}

void CBuffer::createPool() {
    const size_t STRIDE = pixelSize.x * 4;
    const size_t SIZE   = STRIDE * pixelSize.y;

    std::string  name;
    const auto   FD = createPoolFile(SIZE, name);

    if (FD == -1) {
        Debug::log(CRIT, "Unable to create pool file!");
        exit(1);
    }

    const auto DATA = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, FD, 0);
    auto       POOL = makeShared<CCWlShmPool>(g_pHyprpaper->m_pSHM->sendCreatePool(FD, SIZE));
    buffer          = makeShared<CCWlBuffer>(POOL->sendCreateBuffer(0, pixelSize.x, pixelSize.y, STRIDE, WL_SHM_FORMAT_XRGB8888));
    POOL.reset();

    close(FD);

    cpu.data    = DATA;
    cpu.size    = SIZE;
    cpu.surface = cairo_image_surface_create_for_data((unsigned char*)DATA, CAIRO_FORMAT_ARGB32, pixelSize.x, pixelSize.y, STRIDE);
    cpu.cairo   = cairo_create(cpu.surface);
    cpu.name    = name;
}

void CBuffer::destroyPool() {
    cairo_destroy(cpu.cairo);
    cairo_surface_destroy(cpu.surface);
    munmap(cpu.data, cpu.size);
}

void CBuffer::createGpu() {
    uint32_t              format = 0;

    std::vector<uint64_t> modifiers = {};

    // try to find a 10b format+mod first
    for (auto& [fmt, mod] : g_pHyprpaper->m_vDmabufFormats) {
        if (fmt != DRM_FORMAT_XRGB2101010 && fmt != DRM_FORMAT_XBGR2101010)
            continue;

        if (mod == DRM_FORMAT_MOD_LINEAR || mod == DRM_FORMAT_MOD_INVALID)
            continue;

        if (format != 0 && fmt != format)
            continue;

        format = fmt;
        modifiers.emplace_back(mod);
    }

    if (!format) {
        Debug::log(WARN, "No 10-bit DMA format found, trying 8b");
        for (auto& [fmt, mod] : g_pHyprpaper->m_vDmabufFormats) {
            if (fmt != DRM_FORMAT_XRGB8888 && fmt != DRM_FORMAT_XBGR8888)
                continue;

            if (mod == DRM_FORMAT_MOD_LINEAR || mod == DRM_FORMAT_MOD_INVALID)
                continue;

            if (format != 0 && fmt != format)
                continue;

            format = fmt;
            modifiers.emplace_back(mod);
        }
    }

    if (!format) {
        Debug::log(ERR, "Failed to find a dma format for gpu buffer");
        return;
    }

    gpu.bo = gbm_bo_create_with_modifiers2(g_pRenderer->gbmDevice, pixelSize.x, pixelSize.y, format, modifiers.data(), modifiers.size(), GBM_BO_USE_RENDERING);

    if (!gpu.bo) {
        Debug::log(ERR, "Failed to get a bo for gpu buffer, retrying without mods");
        gpu.bo = gbm_bo_create(g_pRenderer->gbmDevice, pixelSize.x, pixelSize.y, format, GBM_BO_USE_RENDERING);
        if (!gpu.bo) {
            Debug::log(ERR, "Failed to get a bo for gpu buffers");
            return;
        }
    }

    gpu.attrs.planes   = gbm_bo_get_plane_count(gpu.bo);
    gpu.attrs.modifier = gbm_bo_get_modifier(gpu.bo);
    gpu.attrs.size     = pixelSize;
    gpu.attrs.format   = format;

    for (size_t i = 0; i < (size_t)gpu.attrs.planes; ++i) {
        gpu.attrs.strides.at(i) = gbm_bo_get_stride_for_plane(gpu.bo, i);
        gpu.attrs.offsets.at(i) = gbm_bo_get_offset(gpu.bo, i);
        gpu.attrs.fds.at(i)     = gbm_bo_get_fd_for_plane(gpu.bo, i);

        if (gpu.attrs.fds.at(i) < 0) {
            Debug::log(ERR, "GBM: Failed to query fd for plane %i", i);
            for (size_t j = 0; j < i; ++j) {
                close(gpu.attrs.fds.at(j));
            }
            gpu.attrs.planes = 0;
            return;
        }
    }

    gpu.attrs.success = true;

    gpu.eglImage = g_pEGL->getEglImage(gpu.attrs);

    if (!gpu.eglImage) {
        Debug::log(ERR, "Failed to get an eglImage for gpu buffer");
        return;
    }

    // send to compositor
    auto PARAMS = makeShared<CCZwpLinuxBufferParamsV1>(g_pHyprpaper->m_pLinuxDmabuf->sendCreateParams());

    for (size_t i = 0; i < (size_t)gpu.attrs.planes; ++i) {
        PARAMS->sendAdd(gpu.attrs.fds.at(i), i, gpu.attrs.offsets.at(i), gpu.attrs.strides.at(i), gpu.attrs.modifier >> 32, gpu.attrs.modifier & 0xFFFFFFFF);
    }

    buffer = makeShared<CCWlBuffer>(PARAMS->sendCreateImmed(pixelSize.x, pixelSize.y, format, (zwpLinuxBufferParamsV1Flags)0));
}

void CBuffer::destroyGpu() {
    g_pEGL->destroyEglImage(gpu.eglImage);
    for (int i = 0; i < gpu.attrs.planes; ++i) {
        close(gpu.attrs.fds.at(i));
    }
    gbm_bo_destroy(gpu.bo);
    gpu.bo = nullptr;
}

CBuffer::CBuffer(const Vector2D& size) : pixelSize(size) {
    if (g_pRenderer->gbmDevice)
        createGpu();
    else
        createPool();
}

CBuffer::~CBuffer() {
    buffer->sendDestroy();

    if (gpu.bo)
        destroyGpu();
    else
        destroyPool();
}