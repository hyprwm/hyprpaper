#include "Hyprpaper.hpp"

CHyprpaper::CHyprpaper() { }

void CHyprpaper::init() {
    g_pConfigManager = std::make_unique<CConfigManager>();

    m_sDisplay = (wl_display *)wl_display_connect(NULL);

    if (!m_sDisplay) {
        Debug::log(CRIT, "No wayland compositor running!");
        exit(1);
        return;
    }

    preloadAllWallpapersFromConfig();

    // run
    wl_registry *registry = wl_display_get_registry(m_sDisplay);
    wl_registry_add_listener(registry, &Events::registryListener, nullptr);

    while (wl_display_dispatch(m_sDisplay) != -1) {
        recheckAllMonitors();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void CHyprpaper::preloadAllWallpapersFromConfig() {
    for (auto& wp : g_pConfigManager->m_dRequestedPreloads) {
        m_mWallpaperTargets[wp] = CWallpaperTarget();
        m_mWallpaperTargets[wp].create(wp);
    }
}

void CHyprpaper::recheckAllMonitors() {
    for (auto& m : m_vMonitors) {
        ensureMonitorHasActiveWallpaper(&m);

        if (m.wantsACK) {
            m.wantsACK = false;
            zwlr_layer_surface_v1_ack_configure(m.pLayerSurface, m.configureSerial);
        }

        if (m.wantsReload) {
            m.wantsReload = false;
            renderWallpaperForMonitor(&m);
        }
    }
}

void CHyprpaper::ensureMonitorHasActiveWallpaper(SMonitor* pMonitor) {
    if (!pMonitor->readyForLS || !pMonitor->hasATarget)
        return;

    auto it = m_mMonitorActiveWallpaperTargets.find(pMonitor);

    if (it == m_mMonitorActiveWallpaperTargets.end()) {
        m_mMonitorActiveWallpaperTargets[pMonitor] = nullptr;
        it = m_mMonitorActiveWallpaperTargets.find(pMonitor);
    }

    if (it->second) 
        return; // has

    // get the target
    for (auto&[mon, path1] : m_mMonitorActiveWallpapers) {
        if (mon == pMonitor->name) {
            for (auto&[path2, target] : m_mWallpaperTargets) {
                if (path1 == path2) {
                    it->second = &target;
                    break;
                }
            }
            break;
        }
    }

    if (!it->second) {
        pMonitor->hasATarget = false;
        Debug::log(WARN, "Monitor %s does not have a target! A wallpaper will not be created.", pMonitor->name.c_str());
        return;
    }

    // create it for thy
    createLSForMonitor(pMonitor);
}

void CHyprpaper::createLSForMonitor(SMonitor* pMonitor) {
    pMonitor->pSurface = wl_compositor_create_surface(m_sCompositor);
    
    if (!pMonitor->pSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a surface!");
        exit(1);
        return;
    }

    const auto PINPUTREGION = wl_compositor_create_region(m_sCompositor);

    if (!PINPUTREGION) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a region!");
        exit(1);
        return;
    }

    wl_surface_set_input_region(pMonitor->pSurface, PINPUTREGION);

    pMonitor->pLayerSurface = zwlr_layer_shell_v1_get_layer_surface(g_pHyprpaper->m_sLayerShell, pMonitor->pSurface, pMonitor->output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "hyprpaper");

    if (!pMonitor->pLayerSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a layersurface!");
        exit(1);
        return;
    }

    zwlr_layer_surface_v1_set_size(pMonitor->pLayerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(pMonitor->pLayerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_exclusive_zone(pMonitor->pLayerSurface, -1);
    zwlr_layer_surface_v1_add_listener(pMonitor->pLayerSurface, &Events::layersurfaceListener, pMonitor);
    wl_surface_commit(pMonitor->pSurface);

    wl_region_destroy(PINPUTREGION);
}

bool CHyprpaper::setCloexec(const int& FD) {
    long flags = fcntl(FD, F_GETFD);
    if (flags == -1) {
        return false;
    }

    if (fcntl(FD, F_SETFD, flags | FD_CLOEXEC) == -1) {
        return false;
    }

    return true;
}

int CHyprpaper::createPoolFile(size_t size, std::string& name) {
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

void CHyprpaper::createBuffer(SPoolBuffer* pBuffer, int32_t w, int32_t h, uint32_t format) {
    const uint STRIDE = w * 4;
    const size_t SIZE = STRIDE * h;

    std::string name;
    const auto FD = createPoolFile(SIZE, name);

    if (FD == -1) {
        Debug::log(CRIT, "Unable to create pool file!");
        exit(1);
    }

    const auto DATA = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, FD, 0);
    const auto POOL = wl_shm_create_pool(g_pHyprpaper->m_sSHM, FD, SIZE);
    pBuffer->buffer = wl_shm_pool_create_buffer(POOL, 0, w, h, STRIDE, format);
    wl_shm_pool_destroy(POOL);

    close(FD);

    pBuffer->size = SIZE;
    pBuffer->data = DATA;
    pBuffer->surface = cairo_image_surface_create_for_data((unsigned char*)DATA, CAIRO_FORMAT_ARGB32, w, h, STRIDE);
    pBuffer->cairo = cairo_create(pBuffer->surface);
}

void CHyprpaper::destroyBuffer(SPoolBuffer* pBuffer) {
    wl_buffer_destroy(pBuffer->buffer);
    cairo_destroy(pBuffer->cairo);
    cairo_surface_destroy(pBuffer->surface);
    munmap(pBuffer->data, pBuffer->size);
}

void CHyprpaper::renderWallpaperForMonitor(SMonitor* pMonitor) {
    SPoolBuffer buffer;
    createBuffer(&buffer, pMonitor->size.x * pMonitor->scale, pMonitor->size.y * pMonitor->scale, WL_SHM_FORMAT_ARGB8888);

    const auto PCAIRO = buffer.cairo;
    cairo_save(PCAIRO);
    cairo_set_operator(PCAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    // render
    // get wp
    const auto PWALLPAPERTARGET = m_mMonitorActiveWallpaperTargets[pMonitor];

    if (!PWALLPAPERTARGET) {
        Debug::log(CRIT, "wallpaper target null in render??");
        exit(1);
    }

    // get scale
    // we always do cover
    float scale;
    Vector2D origin;
    if (pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y) {
        scale = pMonitor->size.x / PWALLPAPERTARGET->m_vSize.x;

        origin.y = - (PWALLPAPERTARGET->m_vSize.y * scale - pMonitor->size.y) / 2.f / scale;

    } else {
        scale = pMonitor->size.y / PWALLPAPERTARGET->m_vSize.y;

        origin.x = - (PWALLPAPERTARGET->m_vSize.x * scale - pMonitor->size.x) / 2.f / scale;
    }

    Debug::log(LOG, "Image data for %s: %s at [%.2f, %.2f], scale: %.2f (original image size: [%i, %i])", pMonitor->name.c_str(), PWALLPAPERTARGET->m_szPath.c_str(), origin.x, origin.y, scale, (int)PWALLPAPERTARGET->m_vSize.x, (int)PWALLPAPERTARGET->m_vSize.y);

    cairo_scale(PCAIRO, scale, scale);
    cairo_set_source_surface(PCAIRO, PWALLPAPERTARGET->m_pCairoSurface, origin.x, origin.y);

    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    wl_surface_set_buffer_scale(pMonitor->pSurface, pMonitor->scale);
    wl_surface_attach(pMonitor->pSurface, buffer.buffer, 0, 0);
    wl_surface_damage_buffer(pMonitor->pSurface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(pMonitor->pSurface);

    // we will not reuse the buffer, so destroy it immediately
    destroyBuffer(&buffer);
}