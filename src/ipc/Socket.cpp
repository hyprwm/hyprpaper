#include "Socket.hpp"
#include "../Hyprpaper.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <pwd.h>
#include <thread>

void CIPCSocket::initialize() {
    std::thread([&]() {
        const auto SOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the hyprpaper Socket. (1) IPC will not work.");
            return;
        }

        sockaddr_un       SERVERADDRESS = {.sun_family = AF_UNIX};

        const auto        HISenv     = getenv("HYPRLAND_INSTANCE_SIGNATURE");
        const auto        RUNTIMEdir = getenv("XDG_RUNTIME_DIR");
        const std::string USERID     = std::to_string(getpwuid(getuid())->pw_uid);

        const auto        USERDIR = RUNTIMEdir ? RUNTIMEdir + std::string{"/hypr/"} : "/run/user/" + USERID + "/hypr/";

        std::string       socketPath = HISenv ? USERDIR + std::string(HISenv) + "/.hyprpaper.sock" : USERDIR + ".hyprpaper.sock";

        if (!HISenv)
            mkdir(USERDIR.c_str(), S_IRWXU);

        unlink(socketPath.c_str());

        strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress = {};
        socklen_t   clientSize    = sizeof(clientAddress);

        char        readBuffer[1024] = {0};

        Debug::log(LOG, "hyprpaper socket started at {} (fd: {})", socketPath, SOCKET);
        while (1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);
            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the hyprpaper Socket. (3) IPC will not work.");
                break;
            } else {
                do {
                    Debug::log(LOG, "Accepted incoming socket connection request on fd {}", ACCEPTEDCONNECTION);
                    std::lock_guard<std::mutex> lg(g_pHyprpaper->m_mtTickMutex);

                    auto                        messageSize              = read(ACCEPTEDCONNECTION, readBuffer, 1024);
                    readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';
                    if (messageSize == 0)
                        break;
                    std::string request(readBuffer);

                    m_szRequest     = request;
                    m_bRequestReady = true;

                    g_pHyprpaper->tick(true);
                    
                    wl_display_roundtrip(g_pHyprpaper->m_sDisplay);
                    
                    while (!m_bReplyReady) { // wait for Hyprpaper to finish processing the request
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    write(ACCEPTEDCONNECTION, m_szReply.c_str(), m_szReply.length());
                    m_bReplyReady = false;
                    m_szReply     = "";

                } while (1);
                Debug::log(LOG, "Closing Accepted Connection");
                close(ACCEPTEDCONNECTION);
            }
        }

        close(SOCKET);
    }).detach();
}

bool CIPCSocket::mainThreadParseRequest() {

    if (!m_bRequestReady)
        return false;

    std::string copy = m_szRequest;

    if (copy == "")
        return false;

    // now we can work on the copy

    Debug::log(LOG, "Received a request: {}", copy);

    // set default reply
    m_szReply       = "ok";
    m_bReplyReady   = true;
    m_bRequestReady = false;

    // config commands
    if (copy.find("wallpaper") == 0 || copy.find("preload") == 0 || copy.find("unload") == 0 || copy.find("reload") == 0) {

        const auto RESULT = g_pConfigManager->config->parseDynamic(copy.substr(0, copy.find_first_of(' ')).c_str(), copy.substr(copy.find_first_of(' ') + 1).c_str());

        if (RESULT.error) {
            m_szReply = RESULT.getError();
            return false;
        }

        return true;
    }

    if (copy.find("listloaded") == 0) {

        const auto numWallpapersLoaded = g_pHyprpaper->m_mWallpaperTargets.size();
        Debug::log(LOG, "numWallpapersLoaded: {}", numWallpapersLoaded);

        if (numWallpapersLoaded == 0) {
            m_szReply = "no wallpapers loaded";
            return false;
        }

        m_szReply           = "";
        long unsigned int i = 0;
        for (auto& [name, target] : g_pHyprpaper->m_mWallpaperTargets) {
            m_szReply += name;
            i++;
            if (i < numWallpapersLoaded)
                m_szReply += '\n'; // dont add newline on last entry
        }

        return true;
    }

    if (copy.find("listactive") == 0) {

        const auto numWallpapersActive = g_pHyprpaper->m_mMonitorActiveWallpapers.size();
        Debug::log(LOG, "numWallpapersActive: {}", numWallpapersActive);

        if (numWallpapersActive == 0) {
            m_szReply = "no wallpapers active";
            return false;
        }

        m_szReply           = "";
        long unsigned int i = 0;
        for (auto& [mon, path1] : g_pHyprpaper->m_mMonitorActiveWallpapers) {
            m_szReply += mon + " = " + path1;
            i++;
            if (i < numWallpapersActive)
                m_szReply += '\n'; // dont add newline on last entry
        }

        return true;
    }

    m_szReply = "invalid command";
    return false;
}
