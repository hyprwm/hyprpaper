#pragma once

#include "../defines.hpp"
#include <mutex>

class CIPCSocket {
  public:
    ~CIPCSocket();

    void initialize();

    bool parseRequest();

    int  fd = -1;

  private:
    std::string processRequest(const std::string& body);
};

inline std::unique_ptr<CIPCSocket> g_pIPCSocket;