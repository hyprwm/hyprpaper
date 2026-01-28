#include "IPC.hpp"
#include "../helpers/Logger.hpp"
#include "../config/WallpaperMatcher.hpp"
#include "../ui/UI.hpp"

#include <filesystem>

using namespace IPC;
using namespace std::string_literals;

constexpr const char*         SOCKET_NAME      = ".hyprpaper.sock";
constexpr const size_t        HP_PROTO_VERSION = 2;

static SP<CHyprpaperCoreImpl> g_coreImpl;

CWallpaperObject::CWallpaperObject(SP<CHyprpaperWallpaperObject>&& obj) : m_object(std::move(obj)) {
    m_object->setDestroy([this]() { std::erase_if(g_IPCSocket->m_wallpaperObjects, [this](const auto& e) { return e.get() == this; }); });
    m_object->setOnDestroy([this]() { std::erase_if(g_IPCSocket->m_wallpaperObjects, [this](const auto& e) { return e.get() == this; }); });

    m_object->setPath([this](const char* s) {
        if (m_inert)
            m_object->error(HYPRPAPER_CORE_WALLPAPER_ERRORS_INERT_WALLPAPER_OBJECT, "Object is inert");

        m_path = s;
    });

    m_object->setFitMode([this](hyprpaperCoreWallpaperFitMode f) {
        if (m_inert)
            m_object->error(HYPRPAPER_CORE_WALLPAPER_ERRORS_INERT_WALLPAPER_OBJECT, "Object is inert");

        if (f > HYPRPAPER_CORE_WALLPAPER_FIT_MODE_TILE)
            m_object->error(HYPRPAPER_CORE_APPLYING_ERROR_UNKNOWN_ERROR, "Invalid fit mode");
        m_fitMode = f;
    });

    m_object->setMonitorName([this](const char* s) {
        if (m_inert)
            m_object->error(HYPRPAPER_CORE_WALLPAPER_ERRORS_INERT_WALLPAPER_OBJECT, "Object is inert");

        m_monitor = s;
    });

    m_object->setApply([this]() {
        if (m_inert)
            m_object->error(HYPRPAPER_CORE_WALLPAPER_ERRORS_INERT_WALLPAPER_OBJECT, "Object is inert");

        apply();
    });
}

static std::string fitModeToStr(hyprpaperCoreWallpaperFitMode m) {
    switch (m) {
        case HYPRPAPER_CORE_WALLPAPER_FIT_MODE_CONTAIN: return "contain";
        case HYPRPAPER_CORE_WALLPAPER_FIT_MODE_COVER: return "cover";
        case HYPRPAPER_CORE_WALLPAPER_FIT_MODE_TILE: return "tile";
        case HYPRPAPER_CORE_WALLPAPER_FIT_MODE_STRETCH: return "fit";
        default: return "cover";
    }
}

void CWallpaperObject::apply() {

    m_inert = true;

    if (!m_monitor.empty() && !g_matcher->outputExists(m_monitor)) {
        m_object->sendFailed(HYPRPAPER_CORE_APPLYING_ERROR_INVALID_MONITOR);
        return;
    }

    if (m_path.empty()) {
        m_object->sendFailed(HYPRPAPER_CORE_APPLYING_ERROR_INVALID_PATH);
        return;
    }

    if (m_path[0] != '/') {
        m_object->sendFailed(HYPRPAPER_CORE_APPLYING_ERROR_INVALID_PATH);
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || ec) {
        m_object->sendFailed(HYPRPAPER_CORE_APPLYING_ERROR_INVALID_PATH);
        return;
    }

    g_matcher->addState(CConfigManager::SSetting{
        .monitor = std::move(m_monitor),
        .fitMode = fitModeToStr(m_fitMode),
        .paths   = std::vector{std::move(m_path)},
    });

    m_object->sendSuccess();
}

CSocket::CSocket() {
    const auto RTDIR = getenv("XDG_RUNTIME_DIR");

    if (!RTDIR)
        return;

    const auto HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS) {
        g_logger->log(LOG_WARN, "not running under hyprland, IPC will be disabled.");
        return;
    }

    m_socketPath = RTDIR + "/hypr/"s + HIS + "/"s + SOCKET_NAME;

    std::error_code ec;
    std::filesystem::remove(m_socketPath, ec);

    m_socket = Hyprwire::IServerSocket::open(m_socketPath);

    if (!m_socket)
        return;

    g_coreImpl = makeShared<CHyprpaperCoreImpl>(HP_PROTO_VERSION, [this](SP<Hyprwire::IObject> obj) {
        auto manager = m_managers.emplace_back(makeShared<CHyprpaperCoreManagerObject>(std::move(obj)));

        manager->setDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] { std::erase(m_managers, weak); });
        manager->setOnDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] { std::erase(m_managers, weak); });

        manager->setGetWallpaperObject([this, weak = WP<CHyprpaperCoreManagerObject>{manager}](uint32_t id) {
            if (!weak)
                return;

            m_wallpaperObjects.emplace_back(makeShared<CWallpaperObject>(
                makeShared<CHyprpaperWallpaperObject>(m_socket->createObject(weak->getObject()->client(), weak->getObject(), "hyprpaper_wallpaper", id))));
        });

        manager->setGetStatusObject([this, weak = WP<CHyprpaperCoreManagerObject>{manager}](uint32_t id) {
            if (!weak)
                return;

            auto x =
                m_statusObjects.emplace_back(makeShared<CHyprpaperStatusObject>(m_socket->createObject(weak->getObject()->client(), weak->getObject(), "hyprpaper_status", id)));

            for (const auto& m : g_ui->targets()) {
                x->sendActiveWallpaper(m->m_monitorName.c_str(), m->m_lastPath.c_str());
            }
        });
    });

    m_socket->addImplementation(g_coreImpl);

    g_ui->backend()->addFd(m_socket->extractLoopFD(), [this]() { m_socket->dispatchEvents(); });
}

void CSocket::onNewDisplay(const std::string& sv) {
    for (const auto& m : m_managers) {
        m->sendAddMonitor(sv.c_str());
    }
}

void CSocket::onRemovedDisplay(const std::string& sv) {
    for (const auto& m : m_managers) {
        m->sendRemoveMonitor(sv.c_str());
    }
}

void CSocket::onWallpaperChanged(const std::string& mon, const std::string& path) {
    for (const auto& so : m_statusObjects) {
        so->sendActiveWallpaper(mon.c_str(), path.c_str());
    }
}
