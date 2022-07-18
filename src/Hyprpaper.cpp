#include "Hyprpaper.hpp"

CHyprpaper::CHyprpaper() { }

void CHyprpaper::init() {

    removeOldHyprpaperImages();

    g_pConfigManager = std::make_unique<CConfigManager>();
    g_pIPCSocket = std::make_unique<CIPCSocket>();

    m_sDisplay = (wl_display *)wl_display_connect(NULL);

    if (!m_sDisplay) {
        Debug::log(CRIT, "No wayland compositor running!");
        exit(1);
        return;
    }

    preloadAllWallpapersFromConfig();

    g_pIPCSocket->initialize();

    // run
    wl_registry *registry = wl_display_get_registry(m_sDisplay);
    wl_registry_add_listener(registry, &Events::registryListener, nullptr);

    std::thread([&]() { // we dispatch wl events cuz we have to
        while (wl_display_dispatch(m_sDisplay) != -1) {
            tick();
        }

        m_bShouldExit = true;
    }).detach();

    while (1) { // we also tick every 1ms for socket and other shit's updates
        tick();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (m_bShouldExit)
            break;
    }
}

void CHyprpaper::tick() {
    m_mtTickMutex.lock();

    preloadAllWallpapersFromConfig();
    ensurePoolBuffersPresent();

    recheckAllMonitors();
   
    g_pIPCSocket->mainThreadParseRequest();

    m_mtTickMutex.unlock();
}

bool CHyprpaper::isPreloaded(const std::string& path) {
    for (auto&[pt, wt] : m_mWallpaperTargets) {
        if (pt == path)
            return true;
    }

    return false;
}

void CHyprpaper::unloadWallpaper(const std::string& path) {
    bool found = false;

    for (auto& [ewp, cls] : m_mWallpaperTargets) {
        if (ewp == path) {
            // found
            found = true;
            break;
        }
    }

    if (!found) {
        Debug::log(LOG, "Cannot unload a target that was not loaded!");
        return;
    }

    // clean buffers
    for (auto it = m_vBuffers.begin(); it != m_vBuffers.end();) {

        if (it->get()->target != path) {
            it++;
            continue;
        }

        const auto PRELOADPATH = it->get()->name;

        Debug::log(LOG, "Unloading target %s, preload path %s", path.c_str(), PRELOADPATH.c_str());

        std::filesystem::remove(PRELOADPATH);

        destroyBuffer(it->get());

        it = m_vBuffers.erase(it);
    }

    m_mWallpaperTargets.erase(path); // will free the cairo surface
}

void CHyprpaper::preloadAllWallpapersFromConfig() {
    if (g_pConfigManager->m_dRequestedPreloads.empty())
        return;

    for (auto& wp : g_pConfigManager->m_dRequestedPreloads) {

        // check if it doesnt exist
        bool exists = false;
        for (auto&[ewp, cls] : m_mWallpaperTargets) {
            if (ewp == wp) {
                Debug::log(LOG, "Ignoring request to preload %s as it already is preloaded!", ewp.c_str());
                exists = true;
                break;
            }
        }

        if (exists)
            continue;

        m_mWallpaperTargets[wp] = CWallpaperTarget();
        m_mWallpaperTargets[wp].create(wp);
    }

    g_pConfigManager->m_dRequestedPreloads.clear();
}

void CHyprpaper::recheckAllMonitors() {
    for (auto& m : m_vMonitors) {
        recheckMonitor(m.get());
    }
}

void CHyprpaper::recheckMonitor(SMonitor* pMonitor) {
    ensureMonitorHasActiveWallpaper(pMonitor);

    if (pMonitor->wantsACK) {
        pMonitor->wantsACK = false;
        zwlr_layer_surface_v1_ack_configure(pMonitor->pCurrentLayerSurface->pLayerSurface, pMonitor->configureSerial);
    }

    if (pMonitor->wantsReload) {
        pMonitor->wantsReload = false;
        renderWallpaperForMonitor(pMonitor);
    }
}

void CHyprpaper::removeOldHyprpaperImages() {
    int cleaned = 0;
    uint64_t memoryFreed = 0;

    for (const auto& entry : std::filesystem::directory_iterator(std::string(getenv("XDG_RUNTIME_DIR")))) {
        if (entry.is_directory())
            continue;

        const auto FILENAME = entry.path().filename().string();

        if (FILENAME.contains(".hyprpaper_")) {
            // unlink it

            memoryFreed += entry.file_size();
            if (!std::filesystem::remove(entry.path()))
                Debug::log(LOG, "Couldn't remove %s", entry.path().string().c_str());
            cleaned++;
        }
    }

    if (cleaned != 0) {
        Debug::log(LOG, "Cleaned old hyprpaper preloads (%i), removing %.1fMB", cleaned, ((float)memoryFreed) / 1000000.f);
    }
}

SMonitor* CHyprpaper::getMonitorFromName(const std::string& monname) {
    for (auto& m : m_vMonitors) {
        if (m->name == monname)
            return m.get();
    }

    return nullptr;
}

void CHyprpaper::ensurePoolBuffersPresent() {
    bool anyNewBuffers = false;

    for (auto&[file, wt] : m_mWallpaperTargets) {
        for (auto& m : m_vMonitors) {

            if (m->size == Vector2D())
                continue;

            auto it = std::find_if(m_vBuffers.begin(), m_vBuffers.end(), [&](const std::unique_ptr<SPoolBuffer>& el) {
                return el->target == wt.m_szPath && el->pixelSize == m->size * m->scale;
            });

            if (it == m_vBuffers.end()) {
                // create
                const auto PBUFFER = m_vBuffers.emplace_back(std::make_unique<SPoolBuffer>()).get();

                createBuffer(PBUFFER, m->size.x * m->scale, m->size.y * m->scale, WL_SHM_FORMAT_ARGB8888);

                PBUFFER->target = wt.m_szPath;

                Debug::log(LOG, "Buffer created for target %s, Shared Memory usage: %.1fMB", wt.m_szPath.c_str(), PBUFFER->size / 1000000.f);

                anyNewBuffers = true;
            }
        }
    }

    if (anyNewBuffers) {
        uint64_t bytesUsed = 0;

        for (auto& bf : m_vBuffers) {
            bytesUsed += bf->size;
        }

        Debug::log(LOG, "Total SM usage for all buffers: %.1fMB", bytesUsed / 1000000.f);
    }
}

void CHyprpaper::clearWallpaperFromMonitor(const std::string& monname) {

    const auto PMONITOR = getMonitorFromName(monname);

    if (!PMONITOR)
        return;

    auto it = m_mMonitorActiveWallpaperTargets.find(PMONITOR);

    if (it != m_mMonitorActiveWallpaperTargets.end())
        m_mMonitorActiveWallpaperTargets.erase(it);
    
    if (PMONITOR->pCurrentLayerSurface) {
        
        PMONITOR->pCurrentLayerSurface = nullptr;

        PMONITOR->wantsACK = false;
        PMONITOR->wantsReload = false;
        PMONITOR->initialized = false;
        PMONITOR->readyForLS = true;

        
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

    // create it for thy if it doesnt have
    if (!pMonitor->pCurrentLayerSurface)
        createLSForMonitor(pMonitor);
    else    
        pMonitor->wantsReload = true;
}

void CHyprpaper::createLSForMonitor(SMonitor* pMonitor) {
    pMonitor->pCurrentLayerSurface = pMonitor->layerSurfaces.emplace_back(std::make_unique<CLayerSurface>(pMonitor)).get();
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
    pBuffer->pixelSize = Vector2D(w, h);
    pBuffer->name = name;
}

void CHyprpaper::destroyBuffer(SPoolBuffer* pBuffer) {
    wl_buffer_destroy(pBuffer->buffer);
    cairo_destroy(pBuffer->cairo);
    cairo_surface_destroy(pBuffer->surface);
    munmap(pBuffer->data, pBuffer->size);

    pBuffer->buffer = nullptr;
}

SPoolBuffer* CHyprpaper::getPoolBuffer(SMonitor* pMonitor, CWallpaperTarget* pWallpaperTarget) {
    return std::find_if(m_vBuffers.begin(), m_vBuffers.end(), [&](const std::unique_ptr<SPoolBuffer>& el) {
        return el->target == pWallpaperTarget->m_szPath && el->pixelSize == pMonitor->size * pMonitor->scale;
    })->get();
}

void CHyprpaper::renderWallpaperForMonitor(SMonitor* pMonitor) {
    const auto PWALLPAPERTARGET = m_mMonitorActiveWallpaperTargets[pMonitor];

    if (!PWALLPAPERTARGET) {
        Debug::log(CRIT, "wallpaper target null in render??");
        exit(1);
    }

    auto* PBUFFER = getPoolBuffer(pMonitor, PWALLPAPERTARGET);

    if (!PBUFFER) {
        Debug::log(LOG, "Pool buffer missing for available target??");
        ensurePoolBuffersPresent();

        PBUFFER = getPoolBuffer(pMonitor, PWALLPAPERTARGET);

        if (!PBUFFER) {
            Debug::log(LOG, "Pool buffer failed #2. Ignoring WP.");
            return;
        }
    }

    const auto PCAIRO = PBUFFER->cairo;
    cairo_save(PCAIRO);
    cairo_set_operator(PCAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    // get scale
    // we always do cover
    float scale;
    Vector2D origin;
    if (pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y) {
        scale = pMonitor->size.x * pMonitor->scale / PWALLPAPERTARGET->m_vSize.x;

        origin.y = - (PWALLPAPERTARGET->m_vSize.y * scale - pMonitor->size.y * pMonitor->scale) / 2.f / scale;

    } else {
        scale = pMonitor->size.y * pMonitor->scale / PWALLPAPERTARGET->m_vSize.y;

        origin.x = - (PWALLPAPERTARGET->m_vSize.x * scale - pMonitor->size.x * pMonitor->scale) / 2.f / scale;
    }

    Debug::log(LOG, "Image data for %s: %s at [%.2f, %.2f], scale: %.2f (original image size: [%i, %i])", pMonitor->name.c_str(), PWALLPAPERTARGET->m_szPath.c_str(), origin.x, origin.y, scale, (int)PWALLPAPERTARGET->m_vSize.x, (int)PWALLPAPERTARGET->m_vSize.y);

    cairo_scale(PCAIRO, scale, scale);
    cairo_set_source_surface(PCAIRO, PWALLPAPERTARGET->m_pCairoSurface, origin.x, origin.y);

    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    wl_surface_attach(pMonitor->pCurrentLayerSurface->pSurface, PBUFFER->buffer, 0, 0);
    wl_surface_set_buffer_scale(pMonitor->pCurrentLayerSurface->pSurface, pMonitor->scale);
    wl_surface_damage_buffer(pMonitor->pCurrentLayerSurface->pSurface, 0, 0, pMonitor->size.x, pMonitor->size.y);
    wl_surface_commit(pMonitor->pCurrentLayerSurface->pSurface);

    // check if we dont need to remove a wallpaper
    if (pMonitor->layerSurfaces.size() > 1) {
        for (auto it = pMonitor->layerSurfaces.begin(); it != pMonitor->layerSurfaces.end(); it++) {
            if (pMonitor->pCurrentLayerSurface != it->get()) {
                pMonitor->layerSurfaces.erase(it);
                break;
            }
        }
    }
}