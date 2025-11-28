#pragma once

#include "../helpers/Memory.hpp"
#include <mutex>
#include <string>

class CIPCSocket {
  public:
    void initialize();

    bool mainThreadParseRequest();

  private:
    std::mutex  m_mtRequestMutex;
    std::string m_szRequest = "";
    std::string m_szReply   = "";

    bool        m_bRequestReady = false;
    bool        m_bReplyReady   = false;
};

inline UP<CIPCSocket> g_pIPCSocket;