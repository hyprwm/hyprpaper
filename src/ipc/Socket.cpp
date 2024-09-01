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
    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
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

    if (bind(fd, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't bind the hyprpaper Socket. IPC will not work.");
        fd = -1;
        return;
    }

    // 10 max queued.
    listen(fd, 10);

    Debug::log(LOG, "hyprpaper socket started at %s (fd: %i)", socketPath.c_str(), fd);
}

CIPCSocket::~CIPCSocket() {
    if (fd >= 0)
        close(fd);
}

bool CIPCSocket::parseRequest() {
    sockaddr_in clientAddress = {};
    socklen_t   clientSize    = sizeof(clientAddress);

    char        readBuffer[1024] = {0};

    const auto  ACCEPTEDCONNECTION = accept(fd, (sockaddr*)&clientAddress, &clientSize);
    if (ACCEPTEDCONNECTION < 0) {
        Debug::log(ERR, "Couldn't listen on the hyprpaper Socket. (3)");
        return false;
    } else {

        Debug::log(LOG, "Accepted incoming socket connection request on fd %i", ACCEPTEDCONNECTION);
        std::string body = "";
        do {
            auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
            body += std::string{readBuffer, messageSize};
            if (messageSize < 1024)
                break;
        } while (1);

        auto reply = processRequest(body);
        write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

        Debug::log(LOG, "Closing Accepted Connection");
        close(ACCEPTEDCONNECTION);
    }

    return true;
}

std::string CIPCSocket::processRequest(const std::string& body) {

    std::string reply = "ok";

    Debug::log(LOG, "Received a request: %s", body.c_str());

    // config commands
    if (body.find("wallpaper") == 0 || body.find("preload") == 0 || body.find("unload") == 0 || body.find("reload") == 0) {

        const auto RESULT = g_pConfigManager->config->parseDynamic(body.substr(0, body.find_first_of(' ')).c_str(), body.substr(body.find_first_of(' ') + 1).c_str());

        if (RESULT.error)
            reply = RESULT.getError();

        return reply;
    }

    if (body.find("listloaded") == 0) {

        const auto numWallpapersLoaded = g_pHyprpaper->m_mWallpaperTargets.size();
        Debug::log(LOG, "numWallpapersLoaded: %d", numWallpapersLoaded);

        if (numWallpapersLoaded == 0)
            return "no wallpapers loaded";

        reply               = "";
        long unsigned int i = 0;
        for (auto& [name, target] : g_pHyprpaper->m_mWallpaperTargets) {
            reply += name;
            i++;
            if (i < numWallpapersLoaded)
                reply += '\n'; // dont add newline on last entry
        }

        return reply;
    }

    if (body.find("listactive") == 0) {

        const auto numWallpapersActive = g_pHyprpaper->m_mMonitorActiveWallpapers.size();
        Debug::log(LOG, "numWallpapersActive: %d", numWallpapersActive);

        if (numWallpapersActive == 0)
            return "no wallpapers active";

        reply               = "";
        long unsigned int i = 0;
        for (auto& [mon, path1] : g_pHyprpaper->m_mMonitorActiveWallpapers) {
            reply += mon + " = " + path1;
            i++;
            if (i < numWallpapersActive)
                reply += '\n'; // dont add newline on last entry
        }

        return reply;
    }

    return "invalid command";
}
