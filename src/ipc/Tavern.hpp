#pragma once

#include "../helpers/Memory.hpp"

#include <hyprwire/hyprwire.hpp>
#include <hp_hyprtavern_core_v1-client.hpp>
#include <hyprpaper_core-server.hpp>

namespace IPC {
    class CWallpaperObject;
    class CTavernConnection;

    class CTavernConnection {
      public:
        CTavernConnection();
        ~CTavernConnection() = default;

        void                  init();

        WP<CTavernConnection> m_self;

        bool                  connected();

        void                  onNewDisplay(const std::string& sv);
        void                  onRemovedDisplay(const std::string& sv);

      private:
        struct {
            SP<Hyprwire::IClientSocket>           socket;
            SP<CCHpHyprtavernCoreManagerV1Object> manager;
            SP<CCHpHyprtavernBusObjectV1Object>   busObject;
        } m_tavern;

        struct {
            SP<Hyprwire::IServerSocket>                  socket;

            std::vector<SP<CHyprpaperCoreManagerObject>> managers;
            std::vector<SP<CWallpaperObject>>            wallpaperObjects;
        } m_object;
    };

    inline UP<CTavernConnection> g_tavernConnection;
};