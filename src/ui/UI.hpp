#pragma once

#include <vector>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Null.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>

#include <hyprutils/signal/Listener.hpp>

#include "../helpers/Memory.hpp"

class CWallpaperTarget {
  public:
    CWallpaperTarget(SP<Hyprtoolkit::IOutput> output, const std::string_view& path, Hyprtoolkit::eImageFitMode fitMode = Hyprtoolkit::IMAGE_FIT_MODE_COVER);
    ~CWallpaperTarget() = default;

    CWallpaperTarget(const CWallpaperTarget&) = delete;
    CWallpaperTarget(CWallpaperTarget&)       = delete;
    CWallpaperTarget(CWallpaperTarget&&)      = delete;

    std::string m_monitorName;

  private:
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

    bool                      run();
    SP<Hyprtoolkit::IBackend> backend();

  private:
    void                              targetChanged(const SP<Hyprtoolkit::IOutput>& mon);
    void                              targetChanged(const std::string_view& monName);
    void                              registerOutput(const SP<Hyprtoolkit::IOutput>& mon);

    SP<Hyprtoolkit::IBackend>         m_backend;

    std::vector<SP<CWallpaperTarget>> m_targets;

    struct {
        Hyprutils::Signal::CHyprSignalListener targetChanged;
        Hyprutils::Signal::CHyprSignalListener newMon;
    } m_listeners;
};

inline UP<CUI> g_ui;
