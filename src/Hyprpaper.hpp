#pragma once

#include "config/ConfigManager.hpp"
#include "defines.hpp"
#include "helpers/MiscFunctions.hpp"
#include "helpers/Monitor.hpp"
#include "helpers/PoolBuffer.hpp"
#include "ipc/Socket.hpp"
#include "render/WallpaperTarget.hpp"
#include <mutex>

#include "protocols/cursor-shape-v1.hpp"
#include "protocols/fractional-scale-v1.hpp"
#include "protocols/linux-dmabuf-v1.hpp"
#include "protocols/viewporter.hpp"
#include "protocols/wayland.hpp"
#include "protocols/wlr-layer-shell-unstable-v1.hpp"

struct SWallpaperRenderData {
    bool contain = false;
};

class CHyprpaper {
  public:
    // important
    wl_display*                      m_sDisplay = nullptr;
    SP<CCWlCompositor>               m_pCompositor;
    SP<CCWlShm>                      m_pSHM;
    SP<CCZwlrLayerShellV1>           m_pLayerShell;
    SP<CCWpFractionalScaleManagerV1> m_pFractionalScale;
    SP<CCWpViewporter>               m_pViewporter;
    SP<CCWlSeat>                     m_pSeat;
    SP<CCWlPointer>                  m_pSeatPointer;
    SP<CCWpCursorShapeDeviceV1>      m_pSeatCursorShapeDevice;
    SP<CCWpCursorShapeManagerV1>     m_pCursorShape;

    // init the utility
    CHyprpaper();
    void                                                  init();
    void                                                  tick(bool force);

    std::unordered_map<std::string, CWallpaperTarget>     m_mWallpaperTargets;
    std::unordered_map<std::string, std::string>          m_mMonitorActiveWallpapers;
    std::unordered_map<std::string, SWallpaperRenderData> m_mMonitorWallpaperRenderData;
    std::unordered_map<SMonitor*, CWallpaperTarget*>      m_mMonitorActiveWallpaperTargets;
    std::vector<std::unique_ptr<SPoolBuffer>>             m_vBuffers;
    std::vector<std::unique_ptr<SMonitor>>                m_vMonitors;

    std::string                                           m_szExplicitConfigPath;
    bool                                                  m_bNoFractionalScale = false;
    bool                                                  m_bTileWallpaper= false;

    void                                                  removeOldHyprpaperImages();
    void                                                  preloadAllWallpapersFromConfig();
    void                                                  recheckAllMonitors();
    void                                                  ensureMonitorHasActiveWallpaper(SMonitor*);
    void                                                  createLSForMonitor(SMonitor*);
    void                                                  renderWallpaperForMonitor(SMonitor*);
    void                                                  createBuffer(SPoolBuffer*, int32_t, int32_t, uint32_t);
    void                                                  destroyBuffer(SPoolBuffer*);
    int                                                   createPoolFile(size_t, std::string&);
    bool                                                  setCloexec(const int&);
    void                                                  clearWallpaperFromMonitor(const std::string&);
    SMonitor*                                             getMonitorFromName(const std::string&);
    bool                                                  isPreloaded(const std::string&);
    void                                                  recheckMonitor(SMonitor*);
    void                                                  ensurePoolBuffersPresent();
    SPoolBuffer*                                          getPoolBuffer(SMonitor*, CWallpaperTarget*);
    void                                                  unloadWallpaper(const std::string&);
    void                                                  createSeat(SP<CCWlSeat>);
    bool                                                  lockSingleInstance(); // fails on multi-instance
    void                                                  unlockSingleInstance();

    std::mutex                                            m_mtTickMutex;

    SMonitor*                                             m_pLastMonitor = nullptr;

  private:
    bool m_bShouldExit = false;
};

inline std::unique_ptr<CHyprpaper> g_pHyprpaper;
