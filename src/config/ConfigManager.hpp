#pragma once
#include "../defines.hpp"
#include <hyprlang.hpp>

class CIPCSocket;

class CConfigManager {
  public:
    // gets all the data from the config
    CConfigManager();
    void                               parse();

    std::deque<std::string>            m_dRequestedPreloads;
    std::string                        getMainConfigPath();
    std::string                        trimPath(std::string path);
    std::string                 absolutePath(const std::string& path);

    std::unique_ptr<Hyprlang::CConfig> config;

  private:
    friend class CIPCSocket;
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;
