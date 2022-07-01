#pragma once

#include "defines.hpp"
#include "config/ConfigManager.hpp"
#include "render/WallpaperTarget.hpp"
#include "helpers/Monitor.hpp"
#include "events/Events.hpp"
#include "helpers/PoolBuffer.hpp"

class CHyprpaper {
public:
    // important
    wl_display* m_sDisplay;
    wl_compositor* m_sCompositor;
    wl_shm* m_sSHM;
    zwlr_layer_shell_v1* m_sLayerShell;

    // init the utility
    CHyprpaper();
    void        init();

    std::unordered_map<std::string, CWallpaperTarget> m_mWallpaperTargets;
    std::unordered_map<std::string, std::string> m_mMonitorActiveWallpapers;
    std::unordered_map<SMonitor*, CWallpaperTarget*> m_mMonitorActiveWallpaperTargets;
    std::vector<SMonitor> m_vMonitors;

    void        preloadAllWallpapersFromConfig();
    void        recheckAllMonitors();
    void        ensureMonitorHasActiveWallpaper(SMonitor*);
    void        createLSForMonitor(SMonitor*);
    void        renderWallpaperForMonitor(SMonitor*);
    void        createBuffer(SPoolBuffer*, int32_t, int32_t, uint32_t);
    void        destroyBuffer(SPoolBuffer*);
    int         createPoolFile(size_t, std::string&);
    bool        setCloexec(const int&);
};

inline std::unique_ptr<CHyprpaper> g_pHyprpaper;