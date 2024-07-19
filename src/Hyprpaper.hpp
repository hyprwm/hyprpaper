#pragma once

#include "config/ConfigManager.hpp"
#include "defines.hpp"
#include "helpers/MiscFunctions.hpp"
#include "helpers/Monitor.hpp"
#include "ipc/Socket.hpp"
#include "render/WallpaperTarget.hpp"
#include <mutex>
#include <unordered_map>

struct SWallpaperRenderData {
    bool contain = false;
};

struct SDMABUFFormat {
    uint32_t format   = 0; // invalid
    uint64_t modifier = 0; // linear
};

class CCWlCompositor;
class CCWlShm;
class CCZwlrLayerShellV1;
class CCWpFractionalScaleManagerV1;
class CCWpViewporter;
class CCWlSeat;
class CCWlPointer;
class CCWpCursorShapeDeviceV1;
class CCWpCursorShapeManagerV1;
class CCZwpLinuxDmabufV1;

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
    SP<CCZwpLinuxDmabufV1>           m_pLinuxDmabuf;

    // init the utility
    CHyprpaper();
    void                                                  init();
    void                                                  tick(bool force);

    std::unordered_map<std::string, CWallpaperTarget>     m_mWallpaperTargets;
    std::unordered_map<std::string, std::string>          m_mMonitorActiveWallpapers;
    std::unordered_map<std::string, SWallpaperRenderData> m_mMonitorWallpaperRenderData;
    std::unordered_map<SMonitor*, CWallpaperTarget*>      m_mMonitorActiveWallpaperTargets;
    std::vector<std::unique_ptr<SMonitor>>                m_vMonitors;
    std::vector<SDMABUFFormat>                            m_vDmabufFormats;

    std::string                                           m_szExplicitConfigPath;
    bool                                                  m_bNoFractionalScale = false;
    bool                                                  m_bNoGpu             = false;

    void                                                  removeOldHyprpaperImages();
    void                                                  preloadAllWallpapersFromConfig();
    void                                                  recheckAllMonitors();
    void                                                  ensureMonitorHasActiveWallpaper(SMonitor*);
    void                                                  createLSForMonitor(SMonitor*);
    void                                                  clearWallpaperFromMonitor(const std::string&);
    SMonitor*                                             getMonitorFromName(const std::string&);
    bool                                                  isPreloaded(const std::string&);
    void                                                  recheckMonitor(SMonitor*);
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
