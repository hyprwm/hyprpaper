#include "Socket.hpp"
#include "../Hyprpaper.hpp"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

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

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        char readBuffer[1024] = {0};

        Debug::log(LOG, "hyprpaper socket started at %s (fd: %i)", socketPath.c_str(), SOCKET);

        while(1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            Debug::log(LOG, "Accepted incoming socket connection request on fd %i", ACCEPTEDCONNECTION);

            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the hyprpaper Socket. (3) IPC will not work.");
                break;
            }

            auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
            readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';

            std::string request(readBuffer);

            m_szRequest = request;
            m_bRequestReady = true;

            while (1) {
                if (m_bReplyReady)
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            write(ACCEPTEDCONNECTION, m_szReply.c_str(), m_szReply.length());

            close(ACCEPTEDCONNECTION);

            m_bReplyReady = false;
            m_szReply = "";
        }

        close(SOCKET);
    }).detach();
}

void CIPCSocket::mainThreadParseRequest() {
    
    if (!m_bRequestReady)
        return;

    std::string copy = m_szRequest;

    // now we can work on the copy

    if (copy == "")
        return;

    Debug::log(LOG, "Received a request: %s", copy.c_str());

    // parse
    if (copy.find("wallpaper") == 0 || copy.find("preload") == 0 || copy.find("unload") == 0) {
        g_pConfigManager->parseError = ""; // reset parse error

        g_pConfigManager->parseKeyword(copy.substr(0, copy.find_first_of(' ')), copy.substr(copy.find_first_of(' ') + 1));

        if (g_pConfigManager->parseError != "") {
            m_szReply = g_pConfigManager->parseError;
            m_bReplyReady = true;
            m_bRequestReady = false;
            return;
        }
    }
    else {
        m_szReply = "invalid command";
        m_bReplyReady = true;
        m_bRequestReady = false;
        return;
    }

    m_szReply = "ok";
    m_bReplyReady = true;
    m_bRequestReady = false;
}