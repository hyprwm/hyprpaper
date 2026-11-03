#include "UI.hpp"
#include "../defines.hpp"
#include "../helpers/Logger.hpp"
#include "../helpers/GlobalState.hpp"
#include "../ipc/HyprlandSocket.hpp"
#include "../ipc/IPC.hpp"
#include "../config/WallpaperMatcher.hpp"
#include "../helpers/MonitorLayout.hpp"

#include <algorithm>
#include <random>
#include <hyprtoolkit/core/Output.hpp>

#include <hyprutils/string/String.hpp>
#include <filesystem>
#include <cmath>
#include <cstdlib>
#include <cairo/cairo.h>
#include <hyprgraphics/image/Image.hpp>
#include <png.h>
#include <cstdio>
#include <vector>

CUI::CUI() = default;

CUI::~CUI() {
    for (const auto& g : m_spanGroups) {
        std::error_code ec;
        for (const auto& [mon, path] : g->lastSlice)
            std::filesystem::remove(path, ec);
    }
    m_targets.clear();
}

static std::string_view pruneDesc(const std::string_view& sv) {
    if (sv.contains('('))
        return Hyprutils::String::trim(sv.substr(0, sv.find_last_of('(')));
    return sv;
}

class CWallpaperTarget::CImagesData {
  public:
    CImagesData(Hyprtoolkit::eImageFitMode fitMode, std::vector<std::string> images, const int timeout = 0, std::string order = "default") :
        fitMode(fitMode), images(std::move(images)), order(std::move(order)), timeout(timeout > 0 ? timeout : 30) {}

    const Hyprtoolkit::eImageFitMode fitMode;
    std::vector<std::string>         images;
    const std::string                order;
    const int                        timeout;

    std::string                      nextImage() {
        if (order == "random-shuffle" && current + 1 >= images.size()) {
            std::random_device rd;
            std::mt19937       g(rd());
            std::shuffle(images.begin(), images.end(), g);
            current = 0;
            return images[current];
        }

        current = (current + 1) % images.size();
        return images[current];
    }

  private:
    size_t current = 0;
};

CWallpaperTarget::CWallpaperTarget(SP<Hyprtoolkit::IBackend> backend, SP<Hyprtoolkit::IOutput> output, const std::vector<std::string>& path, Hyprtoolkit::eImageFitMode fitMode,
                                   const int timeout, const std::string& order) : m_monitorName(output->port()), m_backend(backend) {
    static const auto SPLASH_REPLY = HyprlandSocket::getFromSocket("/splash");

    static const auto PENABLESPLASH = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash");
    static const auto PSPLASHOFFSET = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "splash_offset");
    static const auto PSPLASHALPHA  = Hyprlang::CSimpleConfigValue<Hyprlang::FLOAT>(g_config->hyprlang(), "splash_opacity");

    ASSERT(path.size() > 0);

    m_window = Hyprtoolkit::CWindowBuilder::begin()
                   ->type(Hyprtoolkit::HT_WINDOW_LAYER)
                   ->prefferedOutput(output)
                   ->anchor(0xF)
                   ->layer(0)
                   ->preferredSize({0, 0})
                   ->exclusiveZone(-1)
                   ->appClass("hyprpaper")
                   ->commence();

    m_bg = Hyprtoolkit::CRectangleBuilder::begin()
               ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})
               ->color([] { return Hyprtoolkit::CHyprColor{0xFF000000}; })
               ->commence();
    m_null = Hyprtoolkit::CNullBuilder::begin()->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1, 1}})->commence();

    m_image = Hyprtoolkit::CImageBuilder::begin()
                  ->path(std::string{path.front()})
                  ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                  ->sync(true)
                  ->fitMode(fitMode)
                  ->commence();

    m_lastPath = path.front();

    m_image->setPositionMode(Hyprtoolkit::IElement::HT_POSITION_ABSOLUTE);
    m_image->setPositionFlag(Hyprtoolkit::IElement::HT_POSITION_FLAG_CENTER, true);

    if (path.size() > 1) {
        m_imagesData = makeUnique<CImagesData>(fitMode, std::vector<std::string>(path), timeout, order);
        m_timer =
            m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(m_imagesData->timeout)), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);
    }

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

CWallpaperTarget::~CWallpaperTarget() {
    if (m_timer && !m_timer->passed())
        m_timer->cancel();
}

void CWallpaperTarget::stopSlideshow() {
    if (m_timer && !m_timer->passed())
        m_timer->cancel();
    m_timer.reset();
    m_imagesData.reset();
}

void CWallpaperTarget::setImage(const std::string& path, Hyprtoolkit::eImageFitMode fitMode, bool sync) {
    m_lastPath = path;

    m_image->rebuild()
        ->path(std::string{path})
        ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
        ->sync(sync)
        ->fitMode(fitMode)
        ->commence();
}

void CWallpaperTarget::onRepeatTimer() {

    ASSERT(m_imagesData);

    setImage(m_imagesData->nextImage(), m_imagesData->fitMode);

    m_timer =
        m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(m_imagesData->timeout)), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);

    if (IPC::g_IPCSocket)
        IPC::g_IPCSocket->onWallpaperChanged(m_monitorName, m_lastPath);
}

void CUI::registerOutput(const SP<Hyprtoolkit::IOutput>& mon) {
    g_matcher->registerOutput(mon->port(), pruneDesc(mon->desc()));
    if (IPC::g_IPCSocket)
        IPC::g_IPCSocket->onNewDisplay(mon->port());
    mon->m_events.removed.listenStatic([this, m = WP<Hyprtoolkit::IOutput>{mon}] {
        g_matcher->unregisterOutput(m->port());
        if (IPC::g_IPCSocket)
            IPC::g_IPCSocket->onRemovedDisplay(m->port());
        std::erase_if(m_targets, [&m](const auto& e) { return e->m_monitorName == m->port(); });
        std::vector<uint32_t> ids;
        ids.reserve(m_spanGroups.size());
        for (const auto& g : m_spanGroups)
            ids.emplace_back(g->id);
        for (const auto id : ids)
            rebuildSpanGroup(id);
        pruneSpanGroups();
    });
}

bool CUI::run() {
    static const auto PENABLEIPC = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "ipc");

    //
    Hyprtoolkit::IBackend::SBackendCreationData data;
    data.pLogConnection = makeShared<Hyprutils::CLI::CLoggerConnection>(*g_logger);
    data.pLogConnection->setName("hyprtoolkit");
    data.pLogConnection->setLogLevel(g_state->verbose ? LOG_TRACE : LOG_ERR);

    m_backend = Hyprtoolkit::IBackend::createWithData(data);

    if (!m_backend)
        return false;

    if (*PENABLEIPC)
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

namespace {
    bool isSpanFitMode(const std::string_view& sv) {
        return sv == "span" || sv.starts_with("span:");
    }

    std::string spanSubMode(const std::string_view& sv) {
        const auto POS = sv.find(':');
        if (POS == std::string_view::npos)
            return "cover";
        const auto SUB = sv.substr(POS + 1);
        if (SUB == "contain" || SUB == "stretch" || SUB == "cover")
            return std::string{SUB};
        return "cover";
    }

    Hyprtoolkit::eImageFitMode subModeToFit(const std::string& m) {
        if (m == "contain")
            return Hyprtoolkit::IMAGE_FIT_MODE_CONTAIN;
        if (m == "stretch")
            return Hyprtoolkit::IMAGE_FIT_MODE_STRETCH;
        return Hyprtoolkit::IMAGE_FIT_MODE_COVER;
    }

    std::string spanDir() {
        const char*           rt   = getenv("XDG_RUNTIME_DIR");
        std::filesystem::path base = (rt && *rt) ? std::filesystem::path(rt) / "hyprpaper" : std::filesystem::temp_directory_path() / "hyprpaper";
        std::error_code       ec;
        std::filesystem::create_directories(base, ec);
        return base.string();
    }

    // Fast lossless PNG writer: cairo's write_to_png uses zlib level 6 and costs
    // seconds on monitor-sized images. libpng at level 1 + SUB filter is ~10x faster.
    bool writePNGFast(cairo_surface_t* dst, const std::string& outPath) {
        cairo_surface_flush(dst);
        const int      w      = cairo_image_surface_get_width(dst);
        const int      h      = cairo_image_surface_get_height(dst);
        const int      stride = cairo_image_surface_get_stride(dst);
        const uint8_t* data   = cairo_image_surface_get_data(dst);
        if (!data || w <= 0 || h <= 0)
            return false;

        FILE* f = fopen(outPath.c_str(), "wb");
        if (!f)
            return false;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) {
            fclose(f);
            return false;
        }
        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_write_struct(&png, nullptr);
            fclose(f);
            return false;
        }

        // The row buffer is a raw allocation made BEFORE setjmp and never reassigned, so it
        // stays valid (and freeable) if libpng longjmps here on a write error. Nothing with a
        // non-trivial destructor may be live across the jump, so no std::vector.
        uint8_t* row = sc<uint8_t*>(malloc(sc<size_t>(w) * 3));
        if (!row) {
            png_destroy_write_struct(&png, &info);
            fclose(f);
            return false;
        }

        if (setjmp(png_jmpbuf(png))) {
            free(row);
            png_destroy_write_struct(&png, &info);
            fclose(f);
            return false;
        }

        png_init_io(png, f);
        png_set_compression_level(png, 1);
        png_set_filter(png, 0, PNG_FILTER_SUB);
        png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        // cairo ARGB32 is premultiplied BGRA in memory; slices are opaque, so BGRA->RGB is exact.
        for (int y = 0; y < h; ++y) {
            const uint8_t* srow = data + sc<size_t>(y) * stride;
            for (int x = 0; x < w; ++x) {
                row[x * 3 + 0] = srow[x * 4 + 2];
                row[x * 3 + 1] = srow[x * 4 + 1];
                row[x * 3 + 2] = srow[x * 4 + 0];
            }
            png_write_row(png, row);
        }
        png_write_end(png, info);

        free(row);
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return true;
    }

    // Renders monitor `mon`'s portion of the source onto a PNG at `outPath`.
    // The source is mapped onto the whole virtual canvas per `subMode`; each monitor
    // then samples the sub-rectangle that its layout rect covers.
    bool renderSlice(cairo_surface_t* src, double Iw, double Ih, double canvasX0, double canvasY0, double canvasW, double canvasH, const MonitorLayout::SMonitor& mon,
                     const std::string& subMode, const std::string& outPath) {
        const int devW = std::lround(mon.w * mon.scale);
        const int devH = std::lround(mon.h * mon.scale);
        if (devW <= 0 || devH <= 0)
            return false;

        double ox = 0, oy = 0, fx = 0, fy = 0;
        if (subMode == "stretch") {
            fx = canvasW / Iw;
            fy = canvasH / Ih;
        } else {
            const double f = (subMode == "contain") ? std::min(canvasW / Iw, canvasH / Ih) : std::max(canvasW / Iw, canvasH / Ih);
            fx = fy = f;
            ox      = (canvasW - Iw * f) / 2.0;
            oy      = (canvasH - Ih * f) / 2.0;
        }

        cairo_surface_t* dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, devW, devH);
        cairo_t*         cr  = cairo_create(dst);

        // device = (canvas - monitorOrigin) * scale; user space becomes canvas-logical coords
        cairo_scale(cr, mon.scale, mon.scale);
        cairo_translate(cr, -(mon.x - canvasX0), -(mon.y - canvasY0));
        // then place the source image within the canvas
        cairo_translate(cr, ox, oy);
        cairo_scale(cr, fx, fy);
        cairo_set_source_surface(cr, src, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);

        cairo_destroy(cr);
        const bool ok = writePNGFast(dst, outPath);
        cairo_surface_destroy(dst);
        return ok;
    }
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
    const auto TARGET = g_matcher->getSetting(mon->port(), pruneDesc(mon->desc()));

    // NOTE: the span path deliberately does NOT erase the target here — rebuildSpanGroup
    // updates members in place, so erasing would defeat its signature guard and force a
    // full re-render on every startup/config event.

    if (!TARGET) {
        g_logger->log(LOG_DEBUG, "Monitor {} has no target: no wp will be created", mon->port());
        std::erase_if(m_targets, [&mon](const auto& e) { return e->m_monitorName == mon->port(); });
        rebuildOtherSpanGroups(CConfigManager::SETTING_INVALID);
        return;
    }

    const auto& S = TARGET->get();

    if (isSpanFitMode(S.fitMode)) {
        if (!spanGroupFor(S.id)) {
            auto grp     = makeUnique<SSpanGroup>();
            grp->id      = S.id;
            grp->subMode = spanSubMode(S.fitMode);
            grp->paths   = S.paths;
            grp->order   = S.order;
            grp->timeout = S.timeout > 0 ? S.timeout : 30;
            m_spanGroups.emplace_back(std::move(grp));

            if (S.paths.size() > 1)
                m_spanGroups.back()->timer = m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(m_spanGroups.back()->timeout)),
                                                                 [this, id = S.id](ASP<Hyprtoolkit::CTimer> self, void*) { onSpanTimer(id); }, nullptr);
        }
        rebuildSpanGroup(S.id);
        pruneSpanGroups();
        return;
    }

    std::erase_if(m_targets, [&mon](const auto& e) { return e->m_monitorName == mon->port(); });
    m_targets.emplace_back(makeShared<CWallpaperTarget>(m_backend, mon, S.paths, toFitMode(S.fitMode), S.timeout, S.order));
    rebuildOtherSpanGroups(CConfigManager::SETTING_INVALID);
}

const std::vector<SP<CWallpaperTarget>>& CUI::targets() {
    return m_targets;
}

CUI::SSpanGroup* CUI::spanGroupFor(uint32_t id) {
    for (auto& g : m_spanGroups) {
        if (g->id == id)
            return g.get();
    }
    return nullptr;
}

void CUI::removeSpanGroup(uint32_t id) {
    for (auto& g : m_spanGroups) {
        if (g->id != id)
            continue;
        if (g->timer && !g->timer->passed())
            g->timer->cancel();
        std::error_code ec;
        for (const auto& [mon, path] : g->lastSlice)
            std::filesystem::remove(path, ec);
    }
    std::erase_if(m_spanGroups, [id](const auto& g) { return g->id == id; });
}

void CUI::pruneSpanGroups() {
    const auto            OUTPUTS = m_backend->getOutputs();
    std::vector<uint32_t> dead;
    for (auto& g : m_spanGroups) {
        const bool HAS_MEMBER = std::ranges::any_of(OUTPUTS, [&](const auto& o) {
            const auto st = g_matcher->getSetting(o->port(), pruneDesc(o->desc()));
            return st && st->get().id == g->id;
        });
        if (!HAS_MEMBER)
            dead.emplace_back(g->id);
    }
    for (const auto id : dead)
        removeSpanGroup(id);
}

// Rebuilds every span group except `except` (canvas may have changed for survivors).
void CUI::rebuildOtherSpanGroups(uint32_t except) {
    std::vector<uint32_t> ids;
    ids.reserve(m_spanGroups.size());
    for (const auto& g : m_spanGroups) {
        if (g->id != except)
            ids.emplace_back(g->id);
    }
    for (const auto id : ids)
        rebuildSpanGroup(id);
    pruneSpanGroups();
}

void CUI::onSpanTimer(uint32_t id) {
    auto* g = spanGroupFor(id);
    if (!g || g->paths.empty())
        return;

    // advance to the next base image (mirrors CImagesData::nextImage)
    if (g->order == "random-shuffle" && g->current + 1 >= g->paths.size()) {
        std::random_device rd;
        std::mt19937       gen(rd());
        std::shuffle(g->paths.begin(), g->paths.end(), gen);
        g->current = 0;
    } else
        g->current = (g->current + 1) % g->paths.size();

    rebuildSpanGroup(id);

    g = spanGroupFor(id);
    if (!g)
        return;
    g->timer = m_backend->addTimer(std::chrono::milliseconds(std::chrono::seconds(g->timeout)), [this, id](ASP<Hyprtoolkit::CTimer> self, void*) { onSpanTimer(id); }, nullptr);
}

void CUI::rebuildSpanGroup(uint32_t id) {
    if (!spanGroupFor(id))
        return;

    const auto OUTPUTS = m_backend->getOutputs();

    // members = outputs currently resolving to this span setting
    std::vector<SP<Hyprtoolkit::IOutput>> members;
    for (const auto& o : OUTPUTS) {
        const auto st = g_matcher->getSetting(o->port(), pruneDesc(o->desc()));
        if (st && st->get().id == id)
            members.emplace_back(o);
    }

    if (members.empty()) {
        removeSpanGroup(id);
        return;
    }

    auto* g = spanGroupFor(id);
    if (g->paths.empty())
        return;
    const std::string base = g->paths[g->current % g->paths.size()];

    // skip if nothing that affects the output changed since the last render
    std::string sig = g->subMode + "|" + base;
    {
        std::vector<std::string> names;
        names.reserve(members.size());
        for (const auto& o : members)
            names.emplace_back(o->port());
        std::ranges::sort(names);
        for (const auto& n : names)
            sig += "|" + n;
    }
    // Skip only when nothing changed AND every member still has a live target.
    // Defensive: a target can be dropped out-of-band (e.g. the output-removed
    // listener), and such a member must be recreated rather than skipped.
    const bool ALL_PRESENT = std::ranges::all_of(members, [this](const auto& o) {
        return std::ranges::any_of(m_targets, [&o](const auto& e) { return e->m_monitorName == o->port(); });
    });
    if (sig == g->lastSig && ALL_PRESENT)
        return;
    g->lastSig = sig;

    // ensure/update a per-monitor window with the given image + fit mode.
    // updates load async so a slow decode never blocks the event loop.
    auto applyToMonitor = [this](const SP<Hyprtoolkit::IOutput>& o, const std::string& imgPath, Hyprtoolkit::eImageFitMode fit) {
        for (auto& e : m_targets) {
            if (e->m_monitorName == o->port()) {
                e->stopSlideshow(); // in case this target was previously a standalone slideshow
                e->setImage(imgPath, fit, false);
                return;
            }
        }
        m_targets.emplace_back(makeShared<CWallpaperTarget>(m_backend, o, std::vector<std::string>{imgPath}, fit, 0, "default"));
    };

    auto coverFallback = [&]() {
        for (const auto& o : members) {
            applyToMonitor(o, base, Hyprtoolkit::IMAGE_FIT_MODE_COVER);
            if (IPC::g_IPCSocket)
                IPC::g_IPCSocket->onWallpaperChanged(o->port(), base);
        }
    };

    const auto LAYOUT = MonitorLayout::query();
    if (!LAYOUT) {
        g_logger->log(LOG_WARN, "span: no monitor layout available, falling back to cover");
        coverFallback();
        return;
    }

    // gather geometry for every member
    std::vector<MonitorLayout::SMonitor> geo;
    geo.reserve(members.size());
    for (const auto& o : members) {
        const auto it = std::ranges::find_if(*LAYOUT, [&](const auto& mo) { return mo.name == o->port(); });
        if (it == LAYOUT->end()) {
            g_logger->log(LOG_WARN, "span: no geometry for output {}, falling back to cover", o->port());
            coverFallback();
            return;
        }
        geo.emplace_back(*it);
    }

    // single monitor: span degenerates to the matching simple fit mode
    if (members.size() == 1) {
        applyToMonitor(members.front(), base, subModeToFit(g->subMode));
        if (IPC::g_IPCSocket)
            IPC::g_IPCSocket->onWallpaperChanged(members.front()->port(), base);
        return;
    }

    // decode the source once for the whole group
    Hyprgraphics::CImage img(base);
    if (!img.success() || !img.cairoSurface()) {
        g_logger->log(LOG_ERR, "span: failed to decode {}: {}", base, img.getError());
        coverFallback();
        return;
    }
    cairo_surface_t* src   = img.cairoSurface()->cairo();
    const auto       ISIZE = img.cairoSurface()->size();
    const double     IW = ISIZE.x, IH = ISIZE.y;
    if (IW <= 0 || IH <= 0) {
        coverFallback();
        return;
    }

    // virtual canvas = bounding box of member monitors (logical coords)
    double x0 = geo.front().x, y0 = geo.front().y, x1 = geo.front().x + geo.front().w, y1 = geo.front().y + geo.front().h;
    for (const auto& mo : geo) {
        x0 = std::min(x0, mo.x);
        y0 = std::min(y0, mo.y);
        x1 = std::max(x1, mo.x + mo.w);
        y1 = std::max(y1, mo.y + mo.h);
    }
    const double CW = x1 - x0, CH = y1 - y0;

    const std::string DIR = spanDir();
    const uint64_t    seq = ++g->seq;
    for (size_t i = 0; i < members.size(); ++i) {
        const auto&       o       = members[i];
        const auto&       mo      = geo[i];
        const std::string outPath = DIR + "/span-" + std::to_string(id) + "-" + o->port() + "-" + std::to_string(seq) + ".png";

        if (!renderSlice(src, IW, IH, x0, y0, CW, CH, mo, g->subMode, outPath)) {
            g_logger->log(LOG_ERR, "span: failed to render slice for {}", o->port());
            applyToMonitor(o, base, Hyprtoolkit::IMAGE_FIT_MODE_COVER);
            continue;
        }

        applyToMonitor(o, outPath, Hyprtoolkit::IMAGE_FIT_MODE_STRETCH);

        const auto prev = g->lastSlice.find(o->port());
        if (prev != g->lastSlice.end() && prev->second != outPath) {
            std::error_code ec;
            std::filesystem::remove(prev->second, ec);
        }
        g->lastSlice[o->port()] = outPath;

        if (IPC::g_IPCSocket)
            IPC::g_IPCSocket->onWallpaperChanged(o->port(), base);
    }
}
