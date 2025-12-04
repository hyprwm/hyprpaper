#pragma once

#include "../helpers/Memory.hpp"

#include <hyprwire/hyprwire.hpp>
#include <hyprpaper_core-server.hpp>

namespace IPC {
    class CWallpaperObject {
      public:
        CWallpaperObject(SP<CHyprpaperWallpaperObject>&& obj);
        ~CWallpaperObject() = default;

      private:
        void                          apply();

        SP<CHyprpaperWallpaperObject> m_object;

        std::string                   m_path;
        hyprpaperCoreWallpaperFitMode m_fitMode = HYPRPAPER_CORE_WALLPAPER_FIT_MODE_COVER;
        std::string                   m_monitor;

        bool                          m_inert = false;
    };

    class CSocket {
      public:
        CSocket();
        ~CSocket() = default;

        void onNewDisplay(const std::string& sv);
        void onRemovedDisplay(const std::string& sv);

      private:
        SP<Hyprwire::IServerSocket>                  m_socket;

        std::string                                  m_socketPath = "";

        std::vector<SP<CHyprpaperCoreManagerObject>> m_managers;
        std::vector<SP<CWallpaperObject>>            m_wallpaperObjects;

        friend class CWallpaperObject;
    };

    inline UP<CSocket> g_IPCSocket;
};