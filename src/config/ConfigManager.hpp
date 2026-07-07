#pragma once

#include "../helpers/Memory.hpp"
#include <hyprlang.hpp>
#include <vector>
#include <string>
#include <unordered_map>

class CConfigManager {
  public:
    CConfigManager(const std::string& configPath);
    ~CConfigManager() = default;

    CConfigManager(const CConfigManager&) = delete;
    CConfigManager(CConfigManager&)       = delete;
    CConfigManager(CConfigManager&&)      = delete;

    struct SSetting {
        std::string              monitor, fitMode;
        std::vector<std::string> paths;
        std::string              order   = "default";
        int                      timeout = 0;
        uint32_t                 id      = 0;
        std::string              shaderPath;
    };

    struct SAnimationConfig {
        bool        enabled  = true;
        float       duration = 1.0f;
    };

    constexpr static const uint32_t SETTING_INVALID = 0;

    bool                            init();
    Hyprlang::CConfig*              hyprlang();

    std::vector<SSetting>           getSettings();

    const std::string&              getCurrentConfigPath() const;

    // Queries hyprland's live animation settings over its IPC socket.
    SAnimationConfig getAnimationConfig(const std::string& name);

  private:
    Hyprlang::CConfig m_config;

    std::string       m_currentConfigPath;

    std::unordered_map<std::string, SAnimationConfig> m_animationCache;
};

inline UP<CConfigManager> g_config;
