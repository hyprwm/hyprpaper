#pragma once

#include "../defines.hpp"
#include <mutex>

class CIPCSocket {
public:
    void        initialize();

    bool        mainThreadParseRequest();

private:

    std::mutex m_mtRequestMutex;
    std::string m_szRequest = "";
    std::string m_szReply = "";

    bool        m_bRequestReady = false;
    bool        m_bReplyReady = false;

};

inline std::unique_ptr<CIPCSocket> g_pIPCSocket;