#include "Hyprpaper.hpp"
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <sys/types.h>

CHyprpaper::CHyprpaper() = default;

static void handleGlobal(CCWlRegistry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        g_pHyprpaper->m_pCompositor = makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_compositor_interface, 4));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        g_pHyprpaper->m_pSHM = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_shm_interface, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        g_pHyprpaper->m_mtTickMutex.lock();

        const auto PMONITOR    = g_pHyprpaper->m_vMonitors.emplace_back(std::make_unique<SMonitor>()).get();
        PMONITOR->wayland_name = name;
        PMONITOR->name         = "";
        PMONITOR->output       = makeShared<CCWlOutput>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_output_interface, 4));
        PMONITOR->registerListeners();

        g_pHyprpaper->m_mtTickMutex.unlock();
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        g_pHyprpaper->createSeat(makeShared<CCWlSeat>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_seat_interface, 1)));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        g_pHyprpaper->m_pLayerShell = makeShared<CCZwlrLayerShellV1>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0 && !g_pHyprpaper->m_bNoFractionalScale) {
        g_pHyprpaper->m_pFractionalScale =
            makeShared<CCWpFractionalScaleManagerV1>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wp_fractional_scale_manager_v1_interface, 1));
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        g_pHyprpaper->m_pViewporter = makeShared<CCWpViewporter>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wp_viewporter_interface, 1));
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        g_pHyprpaper->m_pCursorShape =
            makeShared<CCWpCursorShapeManagerV1>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wp_cursor_shape_manager_v1_interface, 1));
    }
}

static void handleGlobalRemove(CCWlRegistry* registry, uint32_t name) {
    for (auto& m : g_pHyprpaper->m_vMonitors) {
        if (m->wayland_name == name) {
            Debug::log(LOG, "Destroying output %s", m->name.c_str());
            g_pHyprpaper->clearWallpaperFromMonitor(m->name);
            std::erase_if(g_pHyprpaper->m_vMonitors, [&](const auto& other) { return other->wayland_name == name; });
            return;
        }
    }
}

void CHyprpaper::init() {

    if (!lockSingleInstance()) {
        Debug::log(CRIT, "Cannot launch multiple instances of Hyprpaper at once!");
        exit(1);
    }

    removeOldHyprpaperImages();

    m_sDisplay = (wl_display*)wl_display_connect(nullptr);

    if (!m_sDisplay) {
        Debug::log(CRIT, "No wayland compositor running!");
        exit(1);
    }

    // run
    auto REGISTRY = makeShared<CCWlRegistry>((wl_proxy*)wl_display_get_registry(m_sDisplay));
    REGISTRY->setGlobal(::handleGlobal);
    REGISTRY->setGlobalRemove(::handleGlobalRemove);

    wl_display_roundtrip(m_sDisplay);

    while (m_vMonitors.size() < 1 || m_vMonitors[0]->name.empty()) {
        wl_display_dispatch(m_sDisplay);
    }

    g_pConfigManager = std::make_unique<CConfigManager>();
    g_pIPCSocket     = std::make_unique<CIPCSocket>();

    g_pConfigManager->parse();

    preloadAllWallpapersFromConfig();

    if (std::any_cast<Hyprlang::INT>(g_pConfigManager->config->getConfigValue("ipc")))
        g_pIPCSocket->initialize();

    do {
        std::lock_guard<std::mutex> lg(m_mtTickMutex);
        tick(true);
    } while (wl_display_dispatch(m_sDisplay) != -1);

    unlockSingleInstance();
}

void CHyprpaper::tick(bool force) {
    bool reload = g_pIPCSocket && g_pIPCSocket->mainThreadParseRequest();

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
            auto                  real_wp       = std::filesystem::read_symlink(wp);
            std::filesystem::path absolute_path = std::filesystem::path(wp).parent_path() / real_wp;
            absolute_path                       = absolute_path.lexically_normal();
            m_mWallpaperTargets[wp].create(absolute_path);
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

void CHyprpaper::createSeat(SP<CCWlSeat> pSeat) {
    m_pSeat = pSeat;

    pSeat->setCapabilities([this](CCWlSeat* r, wl_seat_capability caps) {
        if (caps & WL_SEAT_CAPABILITY_POINTER) {
            m_pSeatPointer = makeShared<CCWlPointer>(m_pSeat->sendGetPointer());
            if (!m_pCursorShape)
                Debug::log(WARN, "No cursor-shape-v1 support from the compositor: cursor will be blank");
            else
                m_pSeatCursorShapeDevice = makeShared<CCWpCursorShapeDeviceV1>(m_pCursorShape->sendGetPointer(m_pSeatPointer->resource()));

            m_pSeatPointer->setEnter([this](CCWlPointer* r, uint32_t serial, wl_resource* surface, wl_fixed_t x, wl_fixed_t y) {
                if (!m_pCursorShape) {
                    m_pSeatPointer->sendSetCursor(serial, nullptr, 0, 0);
                    return;
                }

                m_pSeatCursorShapeDevice->sendSetShape(serial, wpCursorShapeDeviceV1Shape::WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
            });
        } else
            Debug::log(LOG, "No pointer capability from the compositor");
    });
}

void CHyprpaper::recheckMonitor(SMonitor* pMonitor) {
    ensureMonitorHasActiveWallpaper(pMonitor);

    if (pMonitor->wantsACK) {
        pMonitor->wantsACK = false;
        pMonitor->pCurrentLayerSurface->pLayerSurface->sendAckConfigure(pMonitor->configureSerial);
    }

    if (pMonitor->wantsReload) {
        pMonitor->wantsReload = false;
        renderWallpaperForMonitor(pMonitor);
    }
}

void CHyprpaper::removeOldHyprpaperImages() {
    int      cleaned     = 0;
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
    bool        useDesc = false;
    std::string desc    = "";
    if (monname.find("desc:") == 0) {
        useDesc = true;
        desc    = monname.substr(5);
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

    PMONITOR->hasATarget = true;

    if (PMONITOR->pCurrentLayerSurface) {

        PMONITOR->pCurrentLayerSurface = nullptr;

        PMONITOR->wantsACK    = false;
        PMONITOR->wantsReload = false;
        PMONITOR->initialized = false;
        PMONITOR->readyForLS  = true;
    }
}

void CHyprpaper::ensureMonitorHasActiveWallpaper(SMonitor* pMonitor) {
    if (!pMonitor->readyForLS || !pMonitor->hasATarget)
        return;

    auto it = m_mMonitorActiveWallpaperTargets.find(pMonitor);

    if (it == m_mMonitorActiveWallpaperTargets.end()) {
        m_mMonitorActiveWallpaperTargets[pMonitor] = nullptr;
        it                                         = m_mMonitorActiveWallpaperTargets.find(pMonitor);
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
    const size_t SIZE   = STRIDE * h;

    std::string  name;
    const auto   FD = createPoolFile(SIZE, name);

    if (FD == -1) {
        Debug::log(CRIT, "Unable to create pool file!");
        exit(1);
    }

    const auto DATA = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, FD, 0);
    auto       POOL = makeShared<CCWlShmPool>(g_pHyprpaper->m_pSHM->sendCreatePool(FD, SIZE));
    pBuffer->buffer = makeShared<CCWlBuffer>(POOL->sendCreateBuffer(0, w, h, STRIDE, format));
    POOL.reset();

    close(FD);

    pBuffer->size      = SIZE;
    pBuffer->data      = DATA;
    pBuffer->surface   = cairo_image_surface_create_for_data((unsigned char*)DATA, CAIRO_FORMAT_ARGB32, w, h, STRIDE);
    pBuffer->cairo     = cairo_create(pBuffer->surface);
    pBuffer->pixelSize = Vector2D(w, h);
    pBuffer->name      = name;
}

void CHyprpaper::destroyBuffer(SPoolBuffer* pBuffer) {
    pBuffer->buffer.reset();
    cairo_destroy(pBuffer->cairo);
    cairo_surface_destroy(pBuffer->surface);
    munmap(pBuffer->data, pBuffer->size);

    pBuffer->buffer = nullptr;
}

SPoolBuffer* CHyprpaper::getPoolBuffer(SMonitor* pMonitor, CWallpaperTarget* pWallpaperTarget) {
    const auto IT = std::find_if(m_vBuffers.begin(), m_vBuffers.end(), [&](const std::unique_ptr<SPoolBuffer>& el) {
        auto scale =
            std::round((pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale) *
                       120.0) /
            120.0;
        return el->target == pWallpaperTarget->m_szPath && vectorDeltaLessThan(el->pixelSize, pMonitor->size * scale, 1);
    });

    if (IT == m_vBuffers.end())
        return nullptr;
    return IT->get();
}

void CHyprpaper::renderWallpaperForMonitor(SMonitor* pMonitor) {
    static auto* const PRENDERSPLASH = reinterpret_cast<Hyprlang::INT* const*>(g_pConfigManager->config->getConfigValuePtr("splash")->getDataStaticPtr());
    static auto* const PSPLASHOFFSET = reinterpret_cast<Hyprlang::FLOAT* const*>(g_pConfigManager->config->getConfigValuePtr("splash_offset")->getDataStaticPtr());

    if (!m_mMonitorActiveWallpaperTargets[pMonitor])
        recheckMonitor(pMonitor);

    const auto PWALLPAPERTARGET = m_mMonitorActiveWallpaperTargets[pMonitor];
    const auto CONTAIN          = m_mMonitorWallpaperRenderData[pMonitor->name].contain;

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

    const double   SURFACESCALE = pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale;
    const Vector2D DIMENSIONS   = Vector2D{std::round(pMonitor->size.x * SURFACESCALE), std::round(pMonitor->size.y * SURFACESCALE)};

    const auto     PCAIRO = PBUFFER->cairo;
    cairo_save(PCAIRO);
    cairo_set_operator(PCAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(PCAIRO);
    cairo_restore(PCAIRO);

    // always draw a black background behind the wallpaper
    cairo_set_source_rgb(PCAIRO, 0, 0, 0);
    cairo_rectangle(PCAIRO, 0, 0, DIMENSIONS.x, DIMENSIONS.y);
    cairo_fill(PCAIRO);
    cairo_surface_flush(PBUFFER->surface);

    // get scale
    // we always do cover
    double     scale;
    Vector2D   origin;

    const bool LOWASPECTRATIO = pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y;
    if ((CONTAIN && !LOWASPECTRATIO) || (!CONTAIN && LOWASPECTRATIO)) {
        scale    = DIMENSIONS.x / PWALLPAPERTARGET->m_vSize.x;
        origin.y = -(PWALLPAPERTARGET->m_vSize.y * scale - DIMENSIONS.y) / 2.0 / scale;
    } else {
        scale    = DIMENSIONS.y / PWALLPAPERTARGET->m_vSize.y;
        origin.x = -(PWALLPAPERTARGET->m_vSize.x * scale - DIMENSIONS.x) / 2.0 / scale;
    }

    Debug::log(LOG, "Image data for %s: %s at [%.2f, %.2f], scale: %.2f (original image size: [%i, %i])", pMonitor->name.c_str(), PWALLPAPERTARGET->m_szPath.c_str(), origin.x,
               origin.y, scale, (int)PWALLPAPERTARGET->m_vSize.x, (int)PWALLPAPERTARGET->m_vSize.y);

    cairo_scale(PCAIRO, scale, scale);
    cairo_set_source_surface(PCAIRO, PWALLPAPERTARGET->m_pCairoSurface, origin.x, origin.y);

    cairo_paint(PCAIRO);

    if (**PRENDERSPLASH && getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        auto SPLASH = execAndGet("hyprctl splash");
        SPLASH.pop_back();

        Debug::log(LOG, "Rendering splash: %s", SPLASH.c_str());

        cairo_select_font_face(PCAIRO, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        const auto FONTSIZE = (int)(DIMENSIONS.y / 76.0 / scale);
        cairo_set_font_size(PCAIRO, FONTSIZE);

        static auto* const PSPLASHCOLOR = reinterpret_cast<Hyprlang::INT* const*>(g_pConfigManager->config->getConfigValuePtr("splash_color")->getDataStaticPtr());

        Debug::log(LOG, "Splash color: %x", **PSPLASHCOLOR);

        cairo_set_source_rgba(PCAIRO, ((**PSPLASHCOLOR >> 16) & 0xFF) / 255.0, ((**PSPLASHCOLOR >> 8) & 0xFF) / 255.0, (**PSPLASHCOLOR & 0xFF) / 255.0,
                              ((**PSPLASHCOLOR >> 24) & 0xFF) / 255.0);

        cairo_text_extents_t textExtents;
        cairo_text_extents(PCAIRO, SPLASH.c_str(), &textExtents);

        cairo_move_to(PCAIRO, ((DIMENSIONS.x - textExtents.width * scale) / 2.0) / scale, ((DIMENSIONS.y * (100 - **PSPLASHOFFSET)) / 100 - textExtents.height * scale) / scale);

        Debug::log(LOG, "Splash font size: %d, pos: %.2f, %.2f", FONTSIZE, (DIMENSIONS.x - textExtents.width) / 2.0 / scale,
                   ((DIMENSIONS.y * (100 - **PSPLASHOFFSET)) / 100 - textExtents.height * scale) / scale);

        cairo_show_text(PCAIRO, SPLASH.c_str());

        cairo_surface_flush(PWALLPAPERTARGET->m_pCairoSurface);
    }

    cairo_restore(PCAIRO);

    if (pMonitor->pCurrentLayerSurface) {
        pMonitor->pCurrentLayerSurface->pSurface->sendAttach(PBUFFER->buffer.get(), 0, 0);
        pMonitor->pCurrentLayerSurface->pSurface->sendSetBufferScale(pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? 1 : pMonitor->scale);
        pMonitor->pCurrentLayerSurface->pSurface->sendDamageBuffer(0, 0, 0xFFFF, 0xFFFF);

        // our wps are always opaque
        auto opaqueRegion = makeShared<CCWlRegion>(g_pHyprpaper->m_pCompositor->sendCreateRegion());
        opaqueRegion->sendAdd(0, 0, PBUFFER->pixelSize.x, PBUFFER->pixelSize.y);
        pMonitor->pCurrentLayerSurface->pSurface->sendSetOpaqueRegion(opaqueRegion.get());

        if (pMonitor->pCurrentLayerSurface->pFractionalScaleInfo) {
            Debug::log(LOG, "Submitting viewport dest size %ix%i for %x", static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)),
                       pMonitor->pCurrentLayerSurface);
            pMonitor->pCurrentLayerSurface->pViewport->sendSetDestination(static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)));
        }
        pMonitor->pCurrentLayerSurface->pSurface->sendCommit();
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

    const auto        LOCKFILE = XDG_RUNTIME_DIR + "/hyprpaper.lock";

    if (std::filesystem::exists(LOCKFILE)) {
        std::ifstream ifs(LOCKFILE);
        std::string   content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

        try {
            kill(std::stoull(content), 0);

            if (errno != ESRCH)
                return false;
        } catch (std::exception& e) { ; }
    }

    // create lockfile
    std::ofstream ofs(LOCKFILE, std::ios::trunc);

    ofs << std::to_string(getpid());

    ofs.close();

    return true;
}

void CHyprpaper::unlockSingleInstance() {
    const std::string XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    const auto        LOCKFILE        = XDG_RUNTIME_DIR + "/hyprpaper.lock";
    unlink(LOCKFILE.c_str());
}
