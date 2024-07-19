#include "Hyprpaper.hpp"
#include "render/Renderer.hpp"
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/poll.h>
#include "render/Egl.hpp"

#include "protocols/wayland.hpp"
#include "protocols/linux-dmabuf-v1.hpp"
#include "protocols/wlr-layer-shell-unstable-v1.hpp"
#include "protocols/fractional-scale-v1.hpp"
#include "protocols/viewporter.hpp"
#include "protocols/cursor-shape-v1.hpp"

CHyprpaper::CHyprpaper() = default;

static void handleGlobal(CCWlRegistry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        g_pHyprpaper->m_pCompositor = makeShared<CCWlCompositor>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_compositor_interface, 4));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        g_pHyprpaper->m_pSHM = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_shm_interface, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        const auto PMONITOR    = g_pHyprpaper->m_vMonitors.emplace_back(std::make_unique<SMonitor>()).get();
        PMONITOR->wayland_name = name;
        PMONITOR->name         = "";
        PMONITOR->output       = makeShared<CCWlOutput>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &wl_output_interface, 4));
        PMONITOR->registerListeners();
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
    } else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        g_pHyprpaper->m_pLinuxDmabuf = makeShared<CCZwpLinuxDmabufV1>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &zwp_linux_dmabuf_v1_interface, 3));
        g_pHyprpaper->m_pLinuxDmabuf->setModifier([](CCZwpLinuxDmabufV1* r, uint32_t fmt, uint32_t modHi, uint32_t modLo) {
            g_pHyprpaper->m_vDmabufFormats.emplace_back(SDMABUFFormat{
                .format   = fmt,
                .modifier = (((uint64_t)modLo) << 32) | (uint64_t)modLo,
            });
        });

        wl_display_roundtrip(g_pHyprpaper->m_sDisplay);

        g_pHyprpaper->m_pLinuxDmabuf = makeShared<CCZwpLinuxDmabufV1>((wl_proxy*)wl_registry_bind((wl_registry*)registry->resource(), name, &zwp_linux_dmabuf_v1_interface, 4));
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

    g_pRenderer = std::make_unique<CRenderer>(!m_bNoGpu);

    if (!m_bNoGpu) {
        try {
            if (m_vDmabufFormats.empty()) {
                Debug::log(ERR, "No dmabuf support, using cpu rendering");
                m_bNoGpu = true;
            }
        } catch (std::exception& e) {
            std::cerr << "Failed to create a gpu context: " << e.what() << ", falling back to cpu\n";
            m_bNoGpu = true;
        }
    }

    if (m_bNoGpu && (g_pEGL || g_pRenderer->gbmDevice)) {
        g_pEGL.reset();
        g_pRenderer = std::make_unique<CRenderer>(!m_bNoGpu);
    }

    while (m_vMonitors.size() < 1 || m_vMonitors[0]->name.empty()) {
        wl_display_dispatch(m_sDisplay);
    }

    g_pConfigManager = std::make_unique<CConfigManager>();
    g_pIPCSocket     = std::make_unique<CIPCSocket>();

    g_pConfigManager->parse();

    preloadAllWallpapersFromConfig();

    if (std::any_cast<Hyprlang::INT>(g_pConfigManager->config->getConfigValue("ipc")))
        g_pIPCSocket->initialize();

    pollfd pollFDs[] = {
        {
            .fd     = wl_display_get_fd(m_sDisplay),
            .events = POLLIN,
        },
        {
            .fd     = g_pIPCSocket->fd,
            .events = POLLIN,
        },
    };

    tick(true);

    while (1) {
        int ret = poll(pollFDs, 2, 5000 /* 5 seconds, reasonable. Just in case we need to terminate and the signal fails */);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            Debug::log(CRIT, "[core] Polling fds failed with {}", errno);
            exit(1);
        }

        for (size_t i = 0; i < 2; ++i) {
            if (pollFDs[i].revents & POLLHUP) {
                Debug::log(CRIT, "[core] Disconnected from pollfd id {}", i);
                exit(1);
            }
        }

        if (ret != 0) {
            if (pollFDs[0].revents & POLLIN) { // wayland
                wl_display_flush(m_sDisplay);
                if (wl_display_prepare_read(m_sDisplay) == 0) {
                    wl_display_read_events(m_sDisplay);
                    wl_display_dispatch_pending(m_sDisplay);
                } else
                    wl_display_dispatch(m_sDisplay);
            }

            if (pollFDs[1].revents & POLLIN) { // socket
                if (g_pIPCSocket->parseRequest())
                    tick(true);
            }

            // finalize wayland dispatching. Dispatch pending on the queue
            int ret2 = 0;
            do {
                ret2 = wl_display_dispatch_pending(m_sDisplay);
                wl_display_flush(m_sDisplay);
            } while (ret2 > 0);
        }
    }

    unlockSingleInstance();
}

void CHyprpaper::tick(bool force) {
    if (!force)
        return;

    preloadAllWallpapersFromConfig();

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
        g_pRenderer->renderWallpaperForMonitor(pMonitor);
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
    pMonitor->pCurrentLayerSurface = pMonitor->layerSurfaces.emplace_back(makeShared<CLayerSurface>(pMonitor));
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
