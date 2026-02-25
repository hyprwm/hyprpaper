#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
                     Hyprtoolkit::eImageFitMode fitMode = Hyprtoolkit::IMAGE_FIT_MODE_COVER, const int timeout = 0, const uint32_t triggers = 0,
                     const int fileChangeDebounceMs = 0, const std::string& sourcePath = "");
    ~CWallpaperTarget();

    CWallpaperTarget(const CWallpaperTarget&) = delete;
    CWallpaperTarget(CWallpaperTarget&)       = delete;
    CWallpaperTarget(CWallpaperTarget&&)      = delete;

    void onSignal(int signal);

    std::string m_monitorName, m_lastPath;

  private:
    void onRepeatTimer();
    void onFileWatchTimer();
    void setImagePath(const std::string& path);
    void cycleImage();
    bool supportsSignal(int signal) const;

    class CImagesData;
    class CFileWatchData;

    UP<CImagesData>                    m_imagesData;
    UP<CFileWatchData>                 m_fileWatchData;
    ASP<Hyprtoolkit::CTimer>           m_timer;
    ASP<Hyprtoolkit::CTimer>           m_fileWatchTimer;
    SP<Hyprtoolkit::IBackend>          m_backend;
    uint32_t                           m_triggers = 0;
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
    void                              onSignalFDReadable();

    SP<Hyprtoolkit::IBackend>         m_backend;

    int                               m_signalFD = -1;

    std::vector<SP<CWallpaperTarget>> m_targets;

    struct {
        Hyprutils::Signal::CHyprSignalListener targetChanged;
        Hyprutils::Signal::CHyprSignalListener newMon;
    } m_listeners;
};

inline UP<CUI> g_ui;
