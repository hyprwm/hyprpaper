#pragma once

#include "../helpers/Memory.hpp"
#include <hyprlang.hpp>
#include <vector>

class CConfigManager {
  public:
    CConfigManager(const std::string& configPath);
    ~CConfigManager() = default;

    CConfigManager(const CConfigManager&) = delete;
    CConfigManager(CConfigManager&)       = delete;
    CConfigManager(CConfigManager&&)      = delete;

    struct SSetting {
        std::string monitor, fitMode, path;
        uint32_t    id = 0;
    };

    constexpr static const uint32_t SETTING_INVALID = 0;

    void                            init();
    Hyprlang::CConfig*              hyprlang();

    std::vector<SSetting>           getSettings();

  private:
    Hyprlang::CConfig m_config;

    std::string       m_currentConfigPath;
};

inline UP<CConfigManager> g_config;
