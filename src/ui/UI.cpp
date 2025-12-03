#include "UI.hpp"
#include "../helpers/Logger.hpp"
#include "../ipc/HyprlandSocket.hpp"
#include "../ipc/IPC.hpp"
#include "../config/WallpaperMatcher.hpp"

#include <hyprtoolkit/core/Output.hpp>

CUI::CUI() = default;

CUI::~CUI() {
    m_targets.clear();
}

CWallpaperTarget::CWallpaperTarget(SP<Hyprtoolkit::IOutput> output, const std::string_view& path, Hyprtoolkit::eImageFitMode fitMode) : m_monitorName(output->port()) {
    static const auto SPLASH_REPLY = HyprlandSocket::getFromSocket("/splash");

    static const auto PENABLESPLASH = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash");
    static const auto PSPLASHOFFSET = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash_offset");
    static const auto PSPLASHALPHA  = Hyprlang::CSimpleConfigValue<Hyprlang::FLOAT>(g_config->hyprlang(), "splash_opacity");

    m_window = Hyprtoolkit::CWindowBuilder::begin()
                   ->type(Hyprtoolkit::HT_WINDOW_LAYER)
                   ->prefferedOutput(output)
                   ->anchor(0xF)
                   ->layer(0)
                   ->preferredSize({0, 0})
                   ->exclusiveZone(-1)
                   ->commence();

    m_bg = Hyprtoolkit::CRectangleBuilder::begin()
               ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})
               ->color([] { return Hyprtoolkit::CHyprColor{0xFF000000}; })
               ->commence();
    m_null  = Hyprtoolkit::CNullBuilder::begin()->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})->commence();
    m_image = Hyprtoolkit::CImageBuilder::begin()
                  ->path(std::string{path})
                  ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                  ->sync(true)
                  ->fitMode(fitMode)
                  ->commence();

    m_image->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
    m_image->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_CENTER, true);

    m_window->m_rootElement->addChild(m_bg);
    m_window->m_rootElement->addChild(m_null);
    m_null->addChild(m_image);

    if (!SPLASH_REPLY)
        g_logger->log(LOG_ERR, "Can't get splash: {}", SPLASH_REPLY.error());

    if (SPLASH_REPLY && *PENABLESPLASH) {
        m_splash = Hyprtoolkit::CTextBuilder::begin()
                       ->text(std::string{SPLASH_REPLY.value()})
                       ->fontSize({Hyprtoolkit::CFontSize::HT_FONT_TEXT, 1.15F})
                       ->color([] { return g_ui->backend()->getPalette()->m_colors.text; })
                       ->a(*PSPLASHALPHA)
                       ->commence();
        m_splash->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
        m_splash->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_HCENTER, true);
        m_splash->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_BOTTOM, true);
        m_splash->setAbsolutePosition({0.F, sc<float>(-*PSPLASHOFFSET)});
        m_null->addChild(m_splash);
    }

    m_window->open();
}

void CUI::registerOutput(const SP<Hyprtoolkit::IOutput>& mon) {
    g_matcher->registerOutput(mon->port());
    mon->m_events.removed.listenStatic([this, m = WP<Hyprtoolkit::IOutput>{mon}] {
        g_matcher->unregisterOutput(m->port());
        std::erase_if(m_targets, [&m](const auto& e) { return e->m_monitorName == m->port(); });
    });
}

bool CUI::run() {
    m_backend = Hyprtoolkit::IBackend::create();

    if (!m_backend)
        return false;

    IPC::g_IPCSocket = makeUnique<IPC::CSocket>();

    const auto MONITORS = m_backend->getOutputs();

    for (const auto& m : MONITORS) {
        registerOutput(m);
    }

    m_listeners.newMon = m_backend->m_events.outputAdded.listen([this](SP<Hyprtoolkit::IOutput> mon) { registerOutput(mon); });

    g_logger->log(LOG_DEBUG, "Found {} output(s)", MONITORS.size());

    // load the config now, then bind
    for (const auto& m : MONITORS) {
        targetChanged(m);
    }

    m_listeners.targetChanged = g_matcher->m_events.monitorConfigChanged.listen([this](const std::string_view& m) { targetChanged(m); });

    m_backend->enterLoop();

    return true;
}

SP<Hyprtoolkit::IBackend> CUI::backend() {
    return m_backend;
}

static Hyprtoolkit::eImageFitMode toFitMode(const std::string_view& sv) {
    if (sv.starts_with("contain"))
        return Hyprtoolkit::IMAGE_FIT_MODE_CONTAIN;
    if (sv.starts_with("cover"))
        return Hyprtoolkit::IMAGE_FIT_MODE_COVER;
    if (sv.starts_with("tile"))
        return Hyprtoolkit::IMAGE_FIT_MODE_TILE;
    if (sv.starts_with("fill"))
        return Hyprtoolkit::IMAGE_FIT_MODE_STRETCH;
    return Hyprtoolkit::IMAGE_FIT_MODE_COVER;
}

void CUI::targetChanged(const std::string_view& monName) {
    const auto               MONITORS = m_backend->getOutputs();
    SP<Hyprtoolkit::IOutput> monitor;

    for (const auto& m : MONITORS) {
        if (m->port() != monName)
            continue;

        monitor = m;
    }

    if (!monitor) {
        g_logger->log(LOG_ERR, "targetChanged but {} has no output?", monName);
        return;
    }

    targetChanged(monitor);
}

void CUI::targetChanged(const SP<Hyprtoolkit::IOutput>& mon) {
    const auto TARGET = g_matcher->getSetting(mon->port());

    if (!TARGET) {
        g_logger->log(LOG_DEBUG, "Monitor {} has no target: no wp will be created", mon->port());
        return;
    }

    std::erase_if(m_targets, [&mon](const auto& e) { return e->m_monitorName == mon->port(); });

    m_targets.emplace_back(makeShared<CWallpaperTarget>(mon, TARGET->get().path, toFitMode(TARGET->get().fitMode)));
}
