#include "ConfigManager.hpp"
#include <filesystem>
#include <hyprlang.hpp>
#include <hyprutils/path/Path.hpp>
#include <string>
#include <sys/ucontext.h>
#include "../helpers/Logger.hpp"
#include "WallpaperMatcher.hpp"

using namespace std::string_literals;

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
    m_config.addConfigValue("ipc", Hyprlang::INT{1});

    m_config.addSpecialCategory("wallpaper", Hyprlang::SSpecialCategoryOptions{.key = "monitor"});
    m_config.addSpecialConfigValue("wallpaper", "monitor", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "path", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "fit_mode", Hyprlang::STRING{"cover"});

    m_config.commence();

    auto result = m_config.parse();

    if (result.error)
        g_logger->log(LOG_ERR, "Config has errors:\n{}\nProceeding ignoring faulty entries", result.getError());

    g_matcher->addStates(getSettings());
}

Hyprlang::CConfig* CConfigManager::hyprlang() {
    return &m_config;
}

static std::expected<std::string, std::string> resolvePath(const std::string_view& sv) {
    std::error_code ec;
    const auto      CAN = std::filesystem::canonical(sv, ec);

    if (ec)
        return std::unexpected(std::format("invalid path: {}", ec.message()));

    return CAN;
}

static std::expected<std::string, std::string> getFullPath(const std::string_view& sv) {
    if (sv.empty())
        return std::unexpected("empty path");

    if (sv[0] == '~') {
        static auto HOME = getenv("HOME");
        if (!HOME || HOME[0] == '\0')
            return std::unexpected("home path but no $HOME");

        return resolvePath(std::string{HOME} + "/"s + std::string{sv.substr(1)});
    }

    return resolvePath(sv);
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

        const auto RESOLVE_PATH = getFullPath(path);

        if (!RESOLVE_PATH) {
            g_logger->log(LOG_ERR, "Failed to resolve path {}: {}", path, RESOLVE_PATH.error());
            continue;
        }

        result.emplace_back(SSetting{.monitor = std::move(monitor), .fitMode = std::move(fitMode), .path = RESOLVE_PATH.value()});
    }

    return result;
}
