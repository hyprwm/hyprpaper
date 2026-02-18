#include "UI.hpp"
#include "../config/WallpaperMatcher.hpp"
#include "../defines.hpp"
#include "../helpers/GlobalState.hpp"
#include "../helpers/Logger.hpp"
#include "../ipc/HyprlandSocket.hpp"
#include "../ipc/IPC.hpp"

#include <hyprtoolkit/core/Output.hpp>

#include <hyprutils/string/String.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <string_view>
#include <sys/signalfd.h>
#include <unistd.h>
#include <unordered_map>

using namespace std::chrono_literals;

static constexpr auto FILE_WATCH_POLL_INTERVAL = 500ms;

CUI::CUI() = default;

CUI::~CUI() {
    if (m_signalFD >= 0)
        close(m_signalFD);

    m_targets.clear();
}

static std::string_view pruneDesc(const std::string_view& sv) {
    if (sv.contains('('))
        return Hyprutils::String::trim(sv.substr(0, sv.find_last_of('(')));
    return sv;
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

template <typename T>
static void hashCombine(size_t& seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct SPathSnapshot {
    std::vector<std::string>                 images;
    std::unordered_map<std::string, uint64_t> writeTimes;
    size_t                                   signature = 0;
};

static bool isSupportedImagePath(const std::filesystem::path& path) {
    static constexpr std::array exts{".jpg", ".jpeg", ".png", ".bmp", ".webp", ".svg"};

    auto                        ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return std::ranges::any_of(exts, [&ext](const auto& e) { return ext == e; });
}

static uint64_t toNs(std::filesystem::file_time_type tp) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count());
}

static SPathSnapshot snapshotSourcePath(const std::string& sourcePath) {
    SPathSnapshot      out;

    if (sourcePath.empty())
        return out;

    std::error_code    ec;
    const auto         path = std::filesystem::path{sourcePath};

    const auto addFile = [&out](const std::filesystem::path& filePath) {
        std::error_code ec;
        const auto      mtime = std::filesystem::last_write_time(filePath, ec);
        if (ec)
            return;

        const auto pathStr = filePath.string();
        out.images.emplace_back(pathStr);
        out.writeTimes[pathStr] = toNs(mtime);
    };

    if (std::filesystem::is_directory(path, ec) && !ec) {
        auto directoryIt = std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, ec);
        auto endIt       = std::filesystem::directory_iterator{};

        for (; !ec && directoryIt != endIt; directoryIt.increment(ec)) {
            const auto&     entry = *directoryIt;
            std::error_code iterEc;
            if (!entry.is_regular_file(iterEc) || iterEc)
                continue;
            if (!isSupportedImagePath(entry.path()))
                continue;
            addFile(entry.path());
        }
    }

    ec.clear();
    if (std::filesystem::is_regular_file(path, ec) && !ec && isSupportedImagePath(path))
        addFile(path);

    std::sort(out.images.begin(), out.images.end());

    size_t sig = 0;
    for (const auto& image : out.images) {
        hashCombine(sig, image);
        const auto mtIt = out.writeTimes.find(image);
        if (mtIt != out.writeTimes.end())
            hashCombine(sig, mtIt->second);
    }

    out.signature = sig;
    return out;
}

class CWallpaperTarget::CImagesData {
  public:
    CImagesData(Hyprtoolkit::eImageFitMode fitMode, const std::vector<std::string>& images, const int timeout = 0) :
        fitMode(fitMode), images(images), timeout(timeout > 0 ? timeout : 30) {}

    const Hyprtoolkit::eImageFitMode fitMode;
    const int                        timeout;

    std::string                      nextImage() {
        if (images.empty())
            return "";

        current = (current + 1) % images.size();
        return images[current];
    }

    std::string                      currentImage() const {
        if (images.empty())
            return "";

        return images[current];
    }

    bool                             contains(const std::string& path) const {
        return std::ranges::find(images, path) != images.end();
    }

    bool                             setCurrentImage(const std::string& path) {
        const auto IT = std::ranges::find(images, path);
        if (IT == images.end())
            return false;

        current = std::distance(images.begin(), IT);
        return true;
    }

    void                             updateImages(std::vector<std::string> newImages, const std::string& currentPath) {
        if (newImages.empty()) {
            images.clear();
            current = 0;
            return;
        }

        size_t nextCurrent = 0;

        const auto IT = std::ranges::find(newImages, currentPath);
        if (IT != newImages.end())
            nextCurrent = std::distance(newImages.begin(), IT);

        images  = std::move(newImages);
        current = std::min(nextCurrent, images.size() - 1);
    }

  private:
    std::vector<std::string> images;
    size_t                   current = 0;
};

class CWallpaperTarget::CFileWatchData {
  public:
    CFileWatchData(std::string sourcePath, int debounceMs) : sourcePath(std::move(sourcePath)), debounceMs(std::max(0, debounceMs)) {
        lastSnapshot = snapshotSourcePath(this->sourcePath);
    }

    std::string                           sourcePath;
    int                                   debounceMs = 0;
    SPathSnapshot                         lastSnapshot;
    SPathSnapshot                         pendingSnapshot;
    bool                                  dirty = false;
    std::chrono::steady_clock::time_point lastChange;
};

CWallpaperTarget::CWallpaperTarget(SP<Hyprtoolkit::IBackend> backend, SP<Hyprtoolkit::IOutput> output, const std::vector<std::string>& path,
                                   Hyprtoolkit::eImageFitMode fitMode, const int timeout, const uint32_t triggers, const int fileChangeDebounceMs,
                                   const std::string& sourcePath) :
    m_monitorName(output->port()), m_backend(backend), m_triggers(triggers) {
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

    m_imagesData = makeUnique<CImagesData>(fitMode, path, timeout);

    if (path.size() > 1 && timeout != -1)
        m_timer = m_backend->addTimer(std::chrono::seconds(m_imagesData->timeout), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);

    if ((m_triggers & CConfigManager::SSetting::TRIGGER_FILE_CHANGE) && !sourcePath.empty()) {
        m_fileWatchData  = makeUnique<CFileWatchData>(sourcePath, fileChangeDebounceMs);
        m_fileWatchTimer = m_backend->addTimer(FILE_WATCH_POLL_INTERVAL, [this](ASP<Hyprtoolkit::CTimer> self, void*) { onFileWatchTimer(); }, nullptr);
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

    if (m_fileWatchTimer && !m_fileWatchTimer->passed())
        m_fileWatchTimer->cancel();
}

void CWallpaperTarget::setImagePath(const std::string& path) {
    if (path.empty())
        return;

    m_lastPath = path;

    m_image->rebuild()
        ->path(std::string{m_lastPath})
        ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
        ->sync(true)
        ->fitMode(m_imagesData->fitMode)
        ->commence();

    if (IPC::g_IPCSocket)
        IPC::g_IPCSocket->onWallpaperChanged(m_monitorName, m_lastPath);
}

void CWallpaperTarget::cycleImage() {
    ASSERT(m_imagesData);

    setImagePath(m_imagesData->nextImage());
}

void CWallpaperTarget::onRepeatTimer() {
    cycleImage();

    m_timer = m_backend->addTimer(std::chrono::seconds(m_imagesData->timeout), [this](ASP<Hyprtoolkit::CTimer> self, void*) { onRepeatTimer(); }, nullptr);
}

bool CWallpaperTarget::supportsSignal(int signal) const {
    switch (signal) {
        case SIGHUP: return m_triggers & CConfigManager::SSetting::TRIGGER_SIGHUP;
        case SIGUSR1: return m_triggers & CConfigManager::SSetting::TRIGGER_SIGUSR1;
        case SIGUSR2: return m_triggers & CConfigManager::SSetting::TRIGGER_SIGUSR2;
        default: return false;
    }
}

void CWallpaperTarget::onSignal(int signal) {
    if (!supportsSignal(signal))
        return;

    cycleImage();
}

void CWallpaperTarget::onFileWatchTimer() {
    ASSERT(m_fileWatchData);

    const auto snapshot = snapshotSourcePath(m_fileWatchData->sourcePath);

    const auto now = std::chrono::steady_clock::now();

    if (snapshot.signature != m_fileWatchData->lastSnapshot.signature) {
        m_fileWatchData->pendingSnapshot = snapshot;
        m_fileWatchData->dirty           = true;
        m_fileWatchData->lastChange      = now;
    }

    if (m_fileWatchData->dirty) {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_fileWatchData->lastChange).count();
        if (elapsedMs >= m_fileWatchData->debounceMs) {
            auto& oldSnapshot = m_fileWatchData->lastSnapshot;
            auto  newSnapshot = std::move(m_fileWatchData->pendingSnapshot);

            m_fileWatchData->dirty = false;

            if (!newSnapshot.images.empty()) {
                std::vector<std::string> changedOrAdded;
                changedOrAdded.reserve(newSnapshot.images.size());

                for (const auto& image : newSnapshot.images) {
                    const auto NEW_IT = newSnapshot.writeTimes.find(image);
                    const auto OLD_IT = oldSnapshot.writeTimes.find(image);
                    if (OLD_IT == oldSnapshot.writeTimes.end())
                        changedOrAdded.push_back(image);
                    else if (NEW_IT != newSnapshot.writeTimes.end() && NEW_IT->second != OLD_IT->second)
                        changedOrAdded.push_back(image);
                }

                const bool hadCurrentBefore = m_imagesData->contains(m_lastPath);

                m_imagesData->updateImages(newSnapshot.images, m_lastPath);

                if (!changedOrAdded.empty()) {
                    m_imagesData->setCurrentImage(changedOrAdded.front());
                    setImagePath(m_imagesData->currentImage());
                } else if (!hadCurrentBefore || !m_imagesData->contains(m_lastPath))
                    setImagePath(m_imagesData->currentImage());
            }

            m_fileWatchData->lastSnapshot = std::move(newSnapshot);
        }
    }

    m_fileWatchTimer = m_backend->addTimer(FILE_WATCH_POLL_INTERVAL, [this](ASP<Hyprtoolkit::CTimer> self, void*) { onFileWatchTimer(); }, nullptr);
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
    });
}

void CUI::onSignalFDReadable() {
    if (m_signalFD < 0)
        return;

    signalfd_siginfo fdsi;

    while (true) {
        const auto readBytes = read(m_signalFD, &fdsi, sizeof(fdsi));

        if (readBytes == static_cast<ssize_t>(sizeof(fdsi))) {
            for (const auto& target : m_targets) {
                target->onSignal(static_cast<int>(fdsi.ssi_signo));
            }
            continue;
        }

        if (readBytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;

        if (readBytes < 0)
            g_logger->log(LOG_WARN, "Failed reading signal fd: {}", std::strerror(errno));

        return;
    }
}

bool CUI::run() {
    static const auto PENABLEIPC = Hyprlang::CSimpleConfigValue<Hyprlang::INT>(g_config->hyprlang(), "ipc");

    Hyprtoolkit::IBackend::SBackendCreationData data;
    data.pLogConnection = makeShared<Hyprutils::CLI::CLoggerConnection>(*g_logger);
    data.pLogConnection->setName("hyprtoolkit");
    data.pLogConnection->setLogLevel(g_state->verbose ? LOG_TRACE : LOG_ERR);

    m_backend = Hyprtoolkit::IBackend::createWithData(data);

    if (!m_backend)
        return false;

    sigset_t signalMask;
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGHUP);
    sigaddset(&signalMask, SIGUSR1);
    sigaddset(&signalMask, SIGUSR2);

    if (sigprocmask(SIG_BLOCK, &signalMask, nullptr) == 0) {
        m_signalFD = signalfd(-1, &signalMask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (m_signalFD >= 0)
            m_backend->addFd(m_signalFD, [this]() { onSignalFDReadable(); });
        else
            g_logger->log(LOG_WARN, "Failed setting up signalfd: {}", std::strerror(errno));
    } else
        g_logger->log(LOG_WARN, "Failed blocking trigger signals: {}", std::strerror(errno));

    if (*PENABLEIPC)
        IPC::g_IPCSocket = makeUnique<IPC::CSocket>();

    const auto MONITORS = m_backend->getOutputs();

    for (const auto& m : MONITORS) {
        registerOutput(m);
    }

    m_listeners.newMon = m_backend->m_events.outputAdded.listen([this](SP<Hyprtoolkit::IOutput> mon) { registerOutput(mon); });

    g_logger->log(LOG_DEBUG, "Found {} output(s)", MONITORS.size());

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

    if (!TARGET) {
        g_logger->log(LOG_DEBUG, "Monitor {} has no target: no wp will be created", mon->port());
        return;
    }

    std::erase_if(m_targets, [&mon](const auto& e) { return e->m_monitorName == mon->port(); });

    m_targets.emplace_back(
        makeShared<CWallpaperTarget>(m_backend, mon, TARGET->get().paths, toFitMode(TARGET->get().fitMode), TARGET->get().timeout, TARGET->get().triggers,
                                     TARGET->get().fileChangeDebounceMs, TARGET->get().sourcePath));
}

const std::vector<SP<CWallpaperTarget>>& CUI::targets() {
    return m_targets;
}
