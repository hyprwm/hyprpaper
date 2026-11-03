#pragma once

#include <vector>
#include <unordered_map>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Null.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>

#include <hyprutils/signal/Listener.hpp>

#include "../helpers/Memory.hpp"

class CWallpaperTarget {
  public:
    CWallpaperTarget(SP<Hyprtoolkit::IBackend> backend, SP<Hyprtoolkit::IOutput> output, const std::vector<std::string>& path,
                     Hyprtoolkit::eImageFitMode fitMode = Hyprtoolkit::IMAGE_FIT_MODE_COVER, const int timeout = 0, const std::string& order = "default");
    ~CWallpaperTarget();

    CWallpaperTarget(const CWallpaperTarget&) = delete;
    CWallpaperTarget(CWallpaperTarget&)       = delete;
    CWallpaperTarget(CWallpaperTarget&&)      = delete;

    std::string m_monitorName, m_lastPath;

    // Rebuilds the shown image in place (used by slideshow + span groups).
    void        setImage(const std::string& path, Hyprtoolkit::eImageFitMode fitMode, bool sync = true);

    // Detaches any standalone slideshow timer (used when a target is adopted into a span group).
    void        stopSlideshow();

  private:
    void onRepeatTimer();

    class CImagesData;

    UP<CImagesData>                    m_imagesData;
    ASP<Hyprtoolkit::CTimer>           m_timer;
    SP<Hyprtoolkit::IBackend>          m_backend;
    SP<Hyprtoolkit::IWindow>           m_window;
    SP<Hyprtoolkit::CNullElement>      m_null;
    SP<Hyprtoolkit::CRectangleElement> m_bg;
    SP<Hyprtoolkit::CImageElement>     m_image;
    SP<Hyprtoolkit::CTextElement>      m_splash;
};

class CUI {
  public:
    CUI();
    ~CUI();

    bool                                     run();
    SP<Hyprtoolkit::IBackend>                backend();
    const std::vector<SP<CWallpaperTarget>>& targets();

  private:
    void                              targetChanged(const SP<Hyprtoolkit::IOutput>& mon);
    void                              targetChanged(const std::string_view& monName);
    void                              registerOutput(const SP<Hyprtoolkit::IOutput>& mon);

    // span support
    struct SSpanGroup {
        uint32_t                                     id      = 0;
        std::string                                  subMode = "cover"; // cover | contain | stretch
        std::vector<std::string>                     paths;
        std::string                                  order   = "default";
        int                                          timeout = 30;
        size_t                                       current = 0;
        uint64_t                                     seq     = 0;
        ASP<Hyprtoolkit::CTimer>                     timer;
        std::unordered_map<std::string, std::string> lastSlice; // monitor -> last slice file
        std::string                                  lastSig;   // render signature; skip redundant rebuilds
    };

    CUI::SSpanGroup*                  spanGroupFor(uint32_t id);
    void                              rebuildSpanGroup(uint32_t id);
    void                              removeSpanGroup(uint32_t id);
    void                              pruneSpanGroups();
    void                              rebuildOtherSpanGroups(uint32_t except);
    void                              onSpanTimer(uint32_t id);

    SP<Hyprtoolkit::IBackend>         m_backend;

    std::vector<SP<CWallpaperTarget>> m_targets;
    std::vector<UP<SSpanGroup>>       m_spanGroups;

    struct {
        Hyprutils::Signal::CHyprSignalListener targetChanged;
        Hyprutils::Signal::CHyprSignalListener newMon;
    } m_listeners;
};

inline UP<CUI> g_ui;
