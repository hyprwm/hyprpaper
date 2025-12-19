#include "Tavern.hpp"

#include "../helpers/Logger.hpp"
#include "IPC.hpp"
#include "../ui/UI.hpp"

using namespace IPC;

constexpr const uint32_t            TAVERN_PROTOCOL_VERSION = 1;
constexpr const uint32_t            PAPER_PROTOCOL_VERSION  = 1;

static SP<CCHpHyprtavernCoreV1Impl> tavernImpl = makeShared<CCHpHyprtavernCoreV1Impl>(TAVERN_PROTOCOL_VERSION);
static SP<CHyprpaperCoreImpl>       hyprpaperImpl;

CTavernConnection::CTavernConnection() = default;

void CTavernConnection::init() {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");

    if (!XDG_RUNTIME_DIR) {
        g_logger->log(LOG_ERR, "CTavernConnection: no runtime dir");
        return;
    }

    const auto WL_DISPLAY = getenv("WAYLAND_DISPLAY");

    m_tavern.socket = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/hyprtavern/ht.sock"});

    if (!m_tavern.socket) {
        g_logger->log(LOG_ERR, "CTavernConnection: tavern is not serving beer, ignoring tavern");
        return;
    }

    m_tavern.socket->addImplementation(tavernImpl);

    if (!m_tavern.socket->waitForHandshake()) {
        g_logger->log(LOG_ERR, "CTavernConnection: failed a handshake to tavern");
        return;
    }

    const auto SPEC = m_tavern.socket->getSpec(tavernImpl->protocol()->specName());

    if (!SPEC) {
        g_logger->log(LOG_ERR, "CTavernConnection: tavern is bad (no protocol)");
        return;
    }

    m_tavern.manager = makeShared<CCHpHyprtavernCoreManagerV1Object>(m_tavern.socket->bindProtocol(tavernImpl->protocol(), TAVERN_PROTOCOL_VERSION));

    m_tavern.busObject = makeShared<CCHpHyprtavernBusObjectV1Object>(m_tavern.manager->sendGetBusObject("hyprpaper"));

    if (WL_DISPLAY)
        m_tavern.busObject->sendExposeProperty("GLOBAL:WAYLAND_DISPLAY", WL_DISPLAY);
    m_tavern.busObject->sendExposeProtocol("hyprpaper_core", 1);

    m_tavern.busObject->setNewFd([this](int fd) {
        if (!m_object.socket->addClient(fd)) {
            g_logger->log(LOG_ERR, "CTavernConnection: received a fd {} but it's dead", fd);
            return;
        }

        g_logger->log(LOG_DEBUG, "CTavernConnection: new client at fd {}", fd);
    });

    g_ui->backend()->addFd(m_tavern.socket->extractLoopFD(), [this] { //
        m_tavern.socket->dispatchEvents();
    });

    // open empty socket for paper protocol

    m_object.socket = Hyprwire::IServerSocket::open();

    if (!m_object.socket) {
        g_logger->log(LOG_ERR, "CTavernConnection: failed to open an empty socket for incoming connections");
        return;
    }

    hyprpaperImpl = makeShared<CHyprpaperCoreImpl>(PAPER_PROTOCOL_VERSION, [this](SP<Hyprwire::IObject> obj) {
        auto manager = m_object.managers.emplace_back(makeShared<CHyprpaperCoreManagerObject>(std::move(obj)));

        manager->setDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] { std::erase(m_object.managers, weak); });
        manager->setOnDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] { std::erase(m_object.managers, weak); });

        manager->setGetWallpaperObject([this, weak = WP<CHyprpaperCoreManagerObject>{manager}](uint32_t id) {
            if (!weak)
                return;

            m_object.wallpaperObjects.emplace_back(makeShared<CWallpaperObject>(
                makeShared<CHyprpaperWallpaperObject>(m_object.socket->createObject(weak->getObject()->client(), weak->getObject(), "hyprpaper_wallpaper", id))));
        });
    });

    m_object.socket->addImplementation(hyprpaperImpl);

    g_ui->backend()->addFd(m_object.socket->extractLoopFD(), [this] { //
        m_object.socket->dispatchEvents();
    });
}

bool CTavernConnection::connected() {
    return m_object.socket;
}

void CTavernConnection::onNewDisplay(const std::string& sv) {
    for (const auto& p : m_object.managers) {
        p->sendAddMonitor(sv.c_str());
    }
}

void CTavernConnection::onRemovedDisplay(const std::string& sv) {
    for (const auto& p : m_object.managers) {
        p->sendRemoveMonitor(sv.c_str());
    }
}
