#pragma once

#include "../helpers/Memory.hpp"

#include <hyprwire/hyprwire.hpp>
#include <hp_hyprtavern_core_v1-client.hpp>
#include <hyprpaper_core-server.hpp>

namespace IPC {
    class CWallpaperObject;
    class CTavernConnection;

    class CTavernPeer {
      public:
        CTavernPeer(SP<Hyprwire::IServerSocket> sock, WP<CTavernConnection> conn);
        ~CTavernPeer();

        int  extractFD();

        void onNewDisplay(const std::string& sv);
        void onRemovedDisplay(const std::string& sv);

        void dispatch();

      private:
        SP<Hyprwire::IServerSocket>                  m_socket;
        int                                          m_fd = -1;
        SP<CHyprpaperCoreImpl>                       m_impl;
        WP<CTavernConnection>                        m_connection;

        std::vector<SP<CHyprpaperCoreManagerObject>> m_managers;
        std::vector<SP<CWallpaperObject>>            m_wallpaperObjects;
    };

    class CTavernConnection {
      public:
        CTavernConnection();
        ~CTavernConnection() = default;

        void                  init();

        WP<CTavernConnection> m_self;

        bool                  connected();
        void                  dropPeer(CTavernPeer* peer);

        void                  onNewDisplay(const std::string& sv);
        void                  onRemovedDisplay(const std::string& sv);

      private:
        SP<Hyprwire::IClientSocket>           m_socket;
        SP<CCHpHyprtavernCoreManagerV1Object> m_manager;
        SP<CCHpHyprtavernBusObjectV1Object>   m_busObject;

        std::vector<SP<CTavernPeer>>          m_peers;
    };

    inline UP<CTavernConnection> g_tavernConnection;
};