#include "Tavern.hpp"

#include "../helpers/Logger.hpp"
#include "IPC.hpp"
#include "../ui/UI.hpp"

using namespace IPC;

constexpr const uint32_t            PROTOCOL_VERSION = 1;

static SP<CCHpHyprtavernCoreV1Impl> impl = makeShared<CCHpHyprtavernCoreV1Impl>(PROTOCOL_VERSION);

CTavernPeer::CTavernPeer(SP<Hyprwire::IServerSocket> sock, WP<CTavernConnection> conn) : m_socket(sock), m_connection(conn) {
    m_impl = makeShared<CHyprpaperCoreImpl>(1, [this](SP<Hyprwire::IObject> obj) {
        auto manager = m_managers.emplace_back(makeShared<CHyprpaperCoreManagerObject>(std::move(obj)));

        // We disconnect on destroying the manager. TODO: disconnect when the peer disconnects?
        manager->setDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] {
            m_connection->dropPeer(this);
            std::erase(m_managers, weak);
        });
        manager->setOnDestroy([this, weak = WP<CHyprpaperCoreManagerObject>{manager}] {
            m_connection->dropPeer(this);
            std::erase(m_managers, weak);
        });

        manager->setGetWallpaperObject([this, weak = WP<CHyprpaperCoreManagerObject>{manager}](uint32_t id) {
            if (!weak)
                return;

            m_wallpaperObjects.emplace_back(makeShared<CWallpaperObject>(
                makeShared<CHyprpaperWallpaperObject>(m_socket->createObject(weak->getObject()->client(), weak->getObject(), "hyprpaper_wallpaper", id))));
        });
    });

    m_socket->addImplementation(m_impl);
}

CTavernPeer::~CTavernPeer() {
    if (!g_ui || m_fd < 0)
        return;

    g_ui->backend()->removeFd(m_fd);
}

void CTavernPeer::onNewDisplay(const std::string& sv) {
    for (const auto& m : m_managers) {
        m->sendAddMonitor(sv.c_str());
    }
}

void CTavernPeer::onRemovedDisplay(const std::string& sv) {
    for (const auto& m : m_managers) {
        m->sendRemoveMonitor(sv.c_str());
    }
}

int CTavernPeer::extractFD() {
    if (m_fd >= 0)
        return m_fd;

    m_fd = m_socket->extractLoopFD();
    return m_fd;
}

void CTavernPeer::dispatch() {
    if (!m_socket->dispatchEvents())
        m_connection->dropPeer(this);
}

CTavernConnection::CTavernConnection() = default;

void CTavernConnection::init() {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");

    if (!XDG_RUNTIME_DIR) {
        g_logger->log(LOG_ERR, "CTavernConnection: no runtime dir");
        return;
    }

    const auto WL_DISPLAY = getenv("WAYLAND_DISPLAY");

    m_socket = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/hyprtavern/ht.sock"});

    if (!m_socket) {
        g_logger->log(LOG_ERR, "CTavernConnection: tavern is not serving beer, ignoring tavern");
        return;
    }

    m_socket->addImplementation(impl);

    if (!m_socket->waitForHandshake()) {
        g_logger->log(LOG_ERR, "CTavernConnection: failed a handshake to tavern");
        return;
    }

    const auto SPEC = m_socket->getSpec(impl->protocol()->specName());

    if (!SPEC) {
        g_logger->log(LOG_ERR, "CTavernConnection: tavern is bad (no protocol)");
        return;
    }

    m_manager = makeShared<CCHpHyprtavernCoreManagerV1Object>(m_socket->bindProtocol(impl->protocol(), PROTOCOL_VERSION));

    m_busObject = makeShared<CCHpHyprtavernBusObjectV1Object>(m_manager->sendGetBusObject("hyprpaper"));

    if (WL_DISPLAY)
        m_busObject->sendExposeProperty("GLOBAL:WAYLAND_DISPLAY", WL_DISPLAY);
    m_busObject->sendExposeProtocol("hyprpaper_core", 1);

    m_busObject->setNewFd([this](int fd) {
        auto sock = Hyprwire::IServerSocket::open(fd);

        if (!sock) {
            g_logger->log(LOG_ERR, "CTavernConnection: received a fd {} but it's dead", fd);
            return;
        }

        auto x = m_peers.emplace_back(makeShared<CTavernPeer>(sock, m_self));

        g_ui->backend()->addFd(x->extractFD(), [w = WP<CTavernPeer>{x}] {
            if (!w)
                return;

            w->dispatch();
        });
    });

    g_ui->backend()->addFd(m_socket->extractLoopFD(), [this] { //
        m_socket->dispatchEvents();
    });
}

bool CTavernConnection::connected() {
    return m_busObject;
}

void CTavernConnection::dropPeer(CTavernPeer* peer) {
    std::erase_if(m_peers, [peer](const auto& e) { return e.get() == peer; });
}

void CTavernConnection::onNewDisplay(const std::string& sv) {
    for (const auto& p : m_peers) {
        p->onNewDisplay(sv);
    }
}

void CTavernConnection::onRemovedDisplay(const std::string& sv) {
    for (const auto& p : m_peers) {
        p->onRemovedDisplay(sv);
    }
}
