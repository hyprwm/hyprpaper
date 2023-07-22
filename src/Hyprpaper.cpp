#include "Hyprpaper.hpp"
#include <filesystem>
#include <fstream>
#include <sys/types.h>
#include <signal.h>

CHyprpaper::CHyprpaper() = default;

void CHyprpaper::init() {

    if (!lockSingleInstance()) {
        Debug::log(CRIT, "Cannot launch multiple instances of Hyprpaper at once!");
        exit(1);
    }

    removeOldHyprpaperImages();

    g_pConfigManager = std::make_unique<CConfigManager>();
    g_pIPCSocket = std::make_unique<CIPCSocket>();

    m_sDisplay = (wl_display*)wl_display_connect(nullptr);

    if (!m_sDisplay) {
        Debug::log(CRIT, "No wayland compositor running!");
        exit(1);
    }

    preloadAllWallpapersFromConfig();

    if (m_bIPCEnabled)
        g_pIPCSocket->initialize();

    // run
    wl_registry* registry = wl_display_get_registry(m_sDisplay);
    wl_registry_add_listener(registry, &Events::registryListener, nullptr);

    while (wl_display_dispatch(m_sDisplay) != -1) {
        std::lock_guard<std::mutex> lg(m_mtTickMutex);
        tick(true);
    }

    unlockSingleInstance();
}

void CHyprpaper::tick(bool force) {
    bool reload = g_pIPCSocket->mainThreadParseRequest();

    if (!reload && !force)
        return;

    preloadAllWallpapersFromConfig();
    ensurePoolBuffersPresent();

    recheckAllMonitors();
}

bool CHyprpaper::isPreloaded(const std::string& path) {
    for (auto& [pt, wt] : m_mWallpaperTargets) {
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
        for (auto& [ewp, cls] : m_mWallpaperTargets) {
            if (ewp == wp) {
                Debug::log(LOG, "Ignoring request to preload %s as it already is preloaded!", ewp.c_str());
                exists = true;
                break;
            }
        }

        if (exists)
            continue;

        m_mWallpaperTargets[wp] = CWallpaperTarget();
        if (std::filesystem::is_symlink(wp)) {
            auto real_wp = std::filesystem::read_symlink(wp);
            m_mWallpaperTargets[wp].create(real_wp);
        } else {
            m_mWallpaperTargets[wp].create(wp);
        }

    }

    g_pConfigManager->m_dRequestedPreloads.clear();
}

void CHyprpaper::recheckAllMonitors() {
    for (auto& m : m_vMonitors) {
        recheckMonitor(m.get());
    }
}

void CHyprpaper::createSeat(wl_seat* pSeat) {
    wl_seat_add_listener(pSeat, &Events::seatListener, pSeat);
}

void CHyprpaper::recheckMonitor(SMonitor* pMonitor) {
    ensureMonitorHasActiveWallpaper(pMonitor);

    if (pMonitor->wantsACK) {
        pMonitor->wantsACK = false;
        zwlr_layer_surface_v1_ack_configure(pMonitor->pCurrentLayerSurface->pLayerSurface, pMonitor->configureSerial);

        if (!pMonitor->pCurrentLayerSurface->pCursorImg) {
            int XCURSOR_SIZE = 24;
            if (const auto CURSORSIZENV = getenv("XCURSOR_SIZE"); CURSORSIZENV) {
                try {
                    if (XCURSOR_SIZE = std::stoi(CURSORSIZENV); XCURSOR_SIZE <= 0) {
                        throw std::exception();
                    }
                } catch (...) {
                    Debug::log(WARN, "XCURSOR_SIZE environment variable is set incorrectly");
                    XCURSOR_SIZE = 24;
                }
            }

            pMonitor->pCurrentLayerSurface->pCursorTheme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), XCURSOR_SIZE * pMonitor->scale, m_sSHM);
            pMonitor->pCurrentLayerSurface->pCursorImg = wl_cursor_theme_get_cursor(pMonitor->pCurrentLayerSurface->pCursorTheme, "left_ptr")->images[0];
        }
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
    bool useDesc = false;
    std::string desc = "";
    if (monname.find("desc:") == 0) {
        useDesc = true;
        desc = monname.substr(5);
    }

    for (auto& m : m_vMonitors) {
        if (useDesc && m->description.find(desc) == 0)
            return m.get();

        if (m->name == monname)
            return m.get();
    }

    return nullptr;
}

void CHyprpaper::ensurePoolBuffersPresent() {
    bool anyNewBuffers = false;

    for (auto& [file, wt] : m_mWallpaperTargets) {
        for (auto& m : m_vMonitors) {

            if (m->size == Vector2D())
                continue;

            auto it = std::find_if(m_vBuffers.begin(), m_vBuffers.end(), [wt = &wt, &m](const std::unique_ptr<SPoolBuffer>& el) {
                auto scale = std::round((m->pCurrentLayerSurface && m->pCurrentLayerSurface->pFractionalScaleInfo ? m->pCurrentLayerSurface->fScale : m->scale) * 120.0) / 120.0;
                return el->target == wt->m_szPath && vectorDeltaLessThan(el->pixelSize, m->size * scale, 1);
            });

            if (it == m_vBuffers.end()) {
                // create
                const auto PBUFFER = m_vBuffers.emplace_back(std::make_unique<SPoolBuffer>()).get();
                auto scale = std::round((m->pCurrentLayerSurface && m->pCurrentLayerSurface->pFractionalScaleInfo ? m->pCurrentLayerSurface->fScale : m->scale) * 120.0) / 120.0;
                createBuffer(PBUFFER, m->size.x * scale, m->size.y * scale, WL_SHM_FORMAT_ARGB8888);

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
    for (auto& [mon, path1] : m_mMonitorActiveWallpapers) {
        if (mon.find("desc:") != 0)
            continue;

        if (pMonitor->description.find(mon.substr(5)) == 0) {
            for (auto& [path2, target] : m_mWallpaperTargets) {
                if (path1 == path2) {
                    it->second = &target;
                    break;
                }
            }
            break;
        }
    }

    for (auto& [mon, path1] : m_mMonitorActiveWallpapers) {
        if (mon == pMonitor->name) {
            for (auto& [path2, target] : m_mWallpaperTargets) {
                if (path1 == path2) {
                    it->second = &target;
                    break;
                }
            }
            break;
        }
    }

    if (!it->second) {
        // try to find a wildcard
        for (auto& [mon, path1] : m_mMonitorActiveWallpapers) {
            if (mon.empty()) {
                for (auto& [path2, target] : m_mWallpaperTargets) {
                    if (path1 == path2) {
                        it->second = &target;
                        break;
                    }
                }
                break;
            }
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
    const size_t STRIDE = w * 4;
    const size_t SIZE = STRIDE * h;

    std::string name;
    const auto FD = createPoolFile(SIZE, name);

    if (FD == -1) {
        Debug::log(CRIT, "Unable to create pool file!");
        exit(1);
    }

    const auto DATA = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, FD, 0);
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
    const auto IT = std::find_if(m_vBuffers.begin(), m_vBuffers.end(), [&](const std::unique_ptr<SPoolBuffer>& el) {
        auto scale = std::round((pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale) * 120.0) / 120.0;
        return el->target == pWallpaperTarget->m_szPath && vectorDeltaLessThan(el->pixelSize, pMonitor->size * scale, 1);
    });

    if (IT == m_vBuffers.end())
        return nullptr;
    return IT->get();
}

void CHyprpaper::renderWallpaperForMonitor(SMonitor* pMonitor) {
    const auto PWALLPAPERTARGET = m_mMonitorActiveWallpaperTargets[pMonitor];
    const auto CONTAIN = m_mMonitorWallpaperRenderData[pMonitor->name].contain;

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

    const double SURFACESCALE = pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale;
    const Vector2D DIMENSIONS = Vector2D{std::round(pMonitor->size.x * SURFACESCALE), std::round(pMonitor->size.y * SURFACESCALE)};

    const auto PCAIRO = PBUFFER->cairo;
    cairo_save(PCAIRO);
    cairo_set_operator(PCAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    if (CONTAIN) {
        cairo_set_source_rgb(PCAIRO, 0, 0, 0);
        cairo_rectangle(PCAIRO, 0, 0, DIMENSIONS.x, DIMENSIONS.y);

        cairo_fill(PCAIRO);

        cairo_surface_flush(PBUFFER->surface);
    }

    // get scale
    // we always do cover
    float scale;
    Vector2D origin;

    if (!CONTAIN) {
        if (pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y) {
            scale = DIMENSIONS.x / PWALLPAPERTARGET->m_vSize.x;

            origin.y = -(PWALLPAPERTARGET->m_vSize.y * scale - DIMENSIONS.y) / 2.f / scale;

        } else {
            scale = DIMENSIONS.y / PWALLPAPERTARGET->m_vSize.y;

            origin.x = -(PWALLPAPERTARGET->m_vSize.x * scale - DIMENSIONS.x) / 2.f / scale;
        }
    } else {
        if (pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y) {
            scale = (DIMENSIONS.y) / PWALLPAPERTARGET->m_vSize.y;

            origin.x = (DIMENSIONS.x - PWALLPAPERTARGET->m_vSize.x * scale) / 2.0 / scale;
        } else {
            scale = (DIMENSIONS.x) / PWALLPAPERTARGET->m_vSize.x;

            origin.y = (DIMENSIONS.y - PWALLPAPERTARGET->m_vSize.y * scale) / 2.0 / scale;
        }
    }

    Debug::log(LOG, "Image data for %s: %s at [%.2f, %.2f], scale: %.2f (original image size: [%i, %i])", pMonitor->name.c_str(), PWALLPAPERTARGET->m_szPath.c_str(), origin.x, origin.y, scale, (int)PWALLPAPERTARGET->m_vSize.x, (int)PWALLPAPERTARGET->m_vSize.y);

    cairo_scale(PCAIRO, scale, scale);
    cairo_set_source_surface(PCAIRO, PWALLPAPERTARGET->m_pCairoSurface, origin.x, origin.y);

    cairo_paint(PCAIRO);

    if (g_pHyprpaper->m_bRenderSplash && getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        auto SPLASH = execAndGet("hyprctl splash");
        SPLASH.pop_back();

        Debug::log(LOG, "Rendering splash: %s", SPLASH.c_str());

        cairo_select_font_face(PCAIRO, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        const auto FONTSIZE = (int)(DIMENSIONS.y / 76.0 / scale);
        cairo_set_font_size(PCAIRO, FONTSIZE);

        cairo_set_source_rgba(PCAIRO, 1.0, 1.0, 1.0, 0.32);

        cairo_text_extents_t textExtents;
        cairo_text_extents(PCAIRO, SPLASH.c_str(), &textExtents);

        cairo_move_to(PCAIRO, ((DIMENSIONS.x - textExtents.width * scale) / 2.0) / scale, (DIMENSIONS.y * 0.99 - textExtents.height * scale) / scale);

        Debug::log(LOG, "Splash font size: %d, pos: %.2f, %.2f", FONTSIZE, (DIMENSIONS.x - textExtents.width) / 2.0 / scale, DIMENSIONS.y * 0.95 - textExtents.height / scale);

        cairo_show_text(PCAIRO, SPLASH.c_str());

        cairo_surface_flush(PWALLPAPERTARGET->m_pCairoSurface);
    }

    cairo_restore(PCAIRO);

    if (pMonitor->pCurrentLayerSurface) {
        wl_surface_attach(pMonitor->pCurrentLayerSurface->pSurface, PBUFFER->buffer, 0, 0);
        wl_surface_set_buffer_scale(pMonitor->pCurrentLayerSurface->pSurface, pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? 1 : pMonitor->scale);
        wl_surface_damage_buffer(pMonitor->pCurrentLayerSurface->pSurface, 0, 0, 0xFFFF, 0xFFFF);
        if (pMonitor->pCurrentLayerSurface->pFractionalScaleInfo) {
            Debug::log(LOG, "Submitting viewport dest size %ix%i for %x", static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)), pMonitor->pCurrentLayerSurface);
            wp_viewport_set_destination(pMonitor->pCurrentLayerSurface->pViewport, static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)));
        }
        wl_surface_commit(pMonitor->pCurrentLayerSurface->pSurface);
    }

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

bool CHyprpaper::lockSingleInstance() {
    const std::string XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");

    const auto LOCKFILE = XDG_RUNTIME_DIR + "/hyprpaper.lock";

    if (std::filesystem::exists(LOCKFILE)) {
        std::ifstream ifs(LOCKFILE);
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

        try {
            kill(std::stoull(content), 0);

            if (errno != ESRCH)
                return false;
        } catch (std::exception& e) {
            ;
        }
    }

    // create lockfile
    std::ofstream ofs(LOCKFILE, std::ios::trunc);

    ofs << std::to_string(getpid());

    ofs.close();

    return true;
}

void CHyprpaper::unlockSingleInstance() {
    const std::string XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    const auto LOCKFILE = XDG_RUNTIME_DIR + "/hyprpaper.lock";
    unlink(LOCKFILE.c_str());
}
