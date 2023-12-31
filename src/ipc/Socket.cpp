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

void CIPCSocket::initialize() {
    std::thread([&]() {
        const auto SOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the hyprpaper Socket. (1) IPC will not work.");
            return;
        }

        sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};

        const auto HISenv = getenv("HYPRLAND_INSTANCE_SIGNATURE");

        std::string socketPath = HISenv ? "/tmp/hypr/" + std::string(HISenv) + "/.hyprpaper.sock" : "/tmp/hypr/.hyprpaper.sock";

        if (!HISenv) {
            mkdir("/tmp/hypr", S_IRWXU | S_IRWXG);
        }

        unlink(socketPath.c_str());

        strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress = {};
        socklen_t clientSize = sizeof(clientAddress);

        char readBuffer[1024] = {0};

        Debug::log(LOG, "hyprpaper socket started at %s (fd: %i)", socketPath.c_str(), SOCKET);
        while (1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);
            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the hyprpaper Socket. (3) IPC will not work.");
                break;
            } else {
                do {
                    Debug::log(LOG, "Accepted incoming socket connection request on fd %i", ACCEPTEDCONNECTION);
                    std::lock_guard<std::mutex> lg(g_pHyprpaper->m_mtTickMutex);

                    auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
                    readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';
                    if (messageSize == 0)
                        break;
                    std::string request(readBuffer);

                    m_szRequest = request;
                    m_bRequestReady = true;

                    g_pHyprpaper->tick(true);
                    while (!m_bReplyReady) { // wait for Hyprpaper to finish processing the request
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    write(ACCEPTEDCONNECTION, m_szReply.c_str(), m_szReply.length());
                    m_bReplyReady = false;
                    m_szReply = "";

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

    // now we can work on the copy

    if (copy == "")
        return false;

    Debug::log(LOG, "Received a request: %s", copy.c_str());

    // parse
    if (copy.find("wallpaper") == 0 || copy.find("preload") == 0 || copy.find("unload") == 0) {

        const auto RESULT = g_pConfigManager->config->parseDynamic(copy.substr(0, copy.find_first_of(' ')).c_str(), copy.substr(copy.find_first_of(' ') + 1).c_str());

        if (RESULT.error) {
            m_szReply = RESULT.getError();
            m_bReplyReady = true;
            m_bRequestReady = false;
            return false;
        }
    } else {
        m_szReply = "invalid command";
        m_bReplyReady = true;
        m_bRequestReady = false;
        return false;
    }

    m_szReply = "ok";
    m_bReplyReady = true;
    m_bRequestReady = false;

    return true;
}
