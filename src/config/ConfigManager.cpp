#include "ConfigManager.hpp"
#include <filesystem>
#include <hyprlang.hpp>
#include <hyprutils/path/Path.hpp>
#include <string>
#include <sys/ucontext.h>
#include "../helpers/Logger.hpp"
#include "WallpaperMatcher.hpp"

static std::string getMainConfigPath() {
    static const auto paths = Hyprutils::Path::findConfig("hyprpaper");

    return paths.first.value_or("");
}

CConfigManager::CConfigManager(const std::string& configPath) :
    m_config(configPath.empty() ? getMainConfigPath().c_str() : configPath.c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true}) {
    m_currentConfigPath = configPath.empty() ? getMainConfigPath() : configPath;
}

void CConfigManager::init() {
    m_config.addConfigValue("splash", Hyprlang::INT{1});
    m_config.addConfigValue("splash_offset", Hyprlang::INT{20});
    m_config.addConfigValue("splash_opacity", Hyprlang::FLOAT{0.8});

    m_config.addSpecialCategory("wallpaper", Hyprlang::SSpecialCategoryOptions{.key = "monitor"});
    m_config.addSpecialConfigValue("wallpaper", "monitor", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "path", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "fit_mode", Hyprlang::STRING{"contain"});

    m_config.commence();

    auto result = m_config.parse();

    if (result.error)
        g_logger->log(LOG_ERR, "Config has errors:\n{}\nProceeding ignoring faulty entries", result.getError());

    g_matcher->addStates(getSettings());
}

Hyprlang::CConfig* CConfigManager::hyprlang() {
    return &m_config;
}

std::vector<CConfigManager::SSetting> CConfigManager::getSettings() {
    std::vector<CConfigManager::SSetting> result;

    auto                                  keys = m_config.listKeysForSpecialCategory("wallpaper");
    result.reserve(keys.size());

    for (auto& key : keys) {
        std::string monitor, fitMode, path;

        try {
            monitor = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "monitor", key.c_str()));
            fitMode = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "fit_mode", key.c_str()));
            path    = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "path", key.c_str()));
        } catch (...) {
            g_logger->log(LOG_ERR, "Failed parsing wallpaper for key {}", key);
            continue;
        }

        std::error_code ec;
        auto            canonical = std::filesystem::canonical(path, ec);
        if (ec) {
            g_logger->log(LOG_ERR, "Failed parsing wallpaper for key {}: path {} is not valid", key, path);
            continue;
        }

        result.emplace_back(SSetting{.monitor = std::move(monitor), .fitMode = std::move(fitMode), .path = std::move(canonical), .id = ++m_maxId});
    }

    return result;
}
