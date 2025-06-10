#include "ConfigManager.hpp"
#include "../Hyprpaper.hpp"
#include <hyprutils/path/Path.hpp>
#include <filesystem>

static Hyprlang::CParseResult handleWallpaper(const char* C, const char* V) {
    const std::string      COMMAND = C;
    const std::string      VALUE   = V;
    Hyprlang::CParseResult result;

    if (VALUE.find_first_of(',') == std::string::npos) {
        result.setError("wallpaper failed (syntax)");
        return result;
    }

    auto MONITOR   = VALUE.substr(0, VALUE.find_first_of(','));
    auto WALLPAPER = g_pConfigManager->trimPath(VALUE.substr(VALUE.find_first_of(',') + 1));

    bool contain = false;

    if (WALLPAPER.find("contain:") == 0) {
        WALLPAPER = WALLPAPER.substr(8);
        contain   = true;
    }

    bool tile = false;

    if (WALLPAPER.find("tile:") == 0) {
        WALLPAPER = WALLPAPER.substr(5);
        tile      = true;
    }

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER                        = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    std::error_code ec;

    if (!std::filesystem::exists(WALLPAPER, ec)) {
        result.setError((std::string{"wallpaper failed ("} + (ec ? ec.message() : std::string{"no such file"}) + std::string{": "} + WALLPAPER + std::string{")"}).c_str());
        return result;
    }

    if (std::find(g_pConfigManager->m_dRequestedPreloads.begin(), g_pConfigManager->m_dRequestedPreloads.end(), WALLPAPER) == g_pConfigManager->m_dRequestedPreloads.end() &&
        !g_pHyprpaper->isPreloaded(WALLPAPER)) {
        result.setError("wallpaper failed (not preloaded)");
        return result;
    }

    g_pHyprpaper->clearWallpaperFromMonitor(MONITOR);
    g_pHyprpaper->m_mMonitorActiveWallpapers[MONITOR]            = WALLPAPER;
    g_pHyprpaper->m_mMonitorWallpaperRenderData[MONITOR].contain = contain;
    g_pHyprpaper->m_mMonitorWallpaperRenderData[MONITOR].tile    = tile;

    if (MONITOR.empty()) {
        for (auto& m : g_pHyprpaper->m_vMonitors) {
            if (!m->hasATarget || m->wildcard) {
                g_pHyprpaper->clearWallpaperFromMonitor(m->name);
                g_pHyprpaper->m_mMonitorActiveWallpapers[m->name]            = WALLPAPER;
                g_pHyprpaper->m_mMonitorWallpaperRenderData[m->name].contain = contain;
                g_pHyprpaper->m_mMonitorWallpaperRenderData[m->name].tile    = tile;
            }
        }
    } else {
        const auto PMON = g_pHyprpaper->getMonitorFromName(MONITOR);
        if (PMON)
            PMON->wildcard = false;
    }

    return result;
}

static Hyprlang::CParseResult handlePreload(const char* C, const char* V) {
    const std::string COMMAND   = C;
    const std::string VALUE     = V;
    auto              WALLPAPER = VALUE;

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER                        = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    std::error_code ec;

    if (!std::filesystem::exists(WALLPAPER, ec)) {
        Hyprlang::CParseResult result;
        result.setError(((ec ? ec.message() : std::string{"no such file"}) + std::string{": "} + WALLPAPER).c_str());
        return result;
    }

    g_pConfigManager->m_dRequestedPreloads.emplace_back(WALLPAPER);

    return Hyprlang::CParseResult{};
}

static Hyprlang::CParseResult handleUnloadAll(const char* C, const char* V) {
    const std::string        COMMAND = C;
    const std::string        VALUE   = V;
    std::vector<std::string> toUnload;

    for (auto& [name, target] : g_pHyprpaper->m_mWallpaperTargets) {
        if (VALUE == "unused") {
            bool exists = false;
            for (auto& [mon, target2] : g_pHyprpaper->m_mMonitorActiveWallpaperTargets) {
                if (&target == target2) {
                    exists = true;
                    break;
                }
            }

            if (exists)
                continue;
        }

        toUnload.emplace_back(name);
    }

    for (auto& tu : toUnload)
        g_pHyprpaper->unloadWallpaper(tu);

    return Hyprlang::CParseResult{};
}

static Hyprlang::CParseResult handleUnload(const char* C, const char* V) {
    const std::string COMMAND   = C;
    const std::string VALUE     = V;
    auto              WALLPAPER = VALUE;

    if (VALUE == "all" || VALUE == "unused")
        return handleUnloadAll(C, V);

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER                        = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    g_pHyprpaper->unloadWallpaper(WALLPAPER);

    return Hyprlang::CParseResult{};
}

static Hyprlang::CParseResult handleReload(const char* C, const char* V) {
    const std::string COMMAND = C;
    const std::string VALUE   = V;

    auto              WALLPAPER = g_pConfigManager->trimPath(VALUE.substr(VALUE.find_first_of(',') + 1));

    if (WALLPAPER.find("contain:") == 0) {
        WALLPAPER = WALLPAPER.substr(8);
    }

    if (WALLPAPER.find("tile:") == 0)
        WALLPAPER = WALLPAPER.substr(5);

    auto preloadResult = handlePreload(C, WALLPAPER.c_str());
    if (preloadResult.error)
        return preloadResult;

    auto MONITOR = VALUE.substr(0, VALUE.find_first_of(','));

    if (MONITOR.empty()) {
        for (auto& m : g_pHyprpaper->m_vMonitors) {
            auto OLD_WALLPAPER = g_pHyprpaper->m_mMonitorActiveWallpapers[m->name];
            g_pHyprpaper->unloadWallpaper(OLD_WALLPAPER);
        }
    } else {
        auto OLD_WALLPAPER = g_pHyprpaper->m_mMonitorActiveWallpapers[MONITOR];
        g_pHyprpaper->unloadWallpaper(OLD_WALLPAPER);
    }

    auto wallpaperResult = handleWallpaper(C, V);
    if (wallpaperResult.error)
        return wallpaperResult;

    return Hyprlang::CParseResult{};
}

CConfigManager::CConfigManager() {
    // Initialize the configuration
    // Read file from default location
    // or from an explicit location given by user

    std::string configPath = getMainConfigPath();

    config = std::make_unique<Hyprlang::CConfig>(configPath.c_str(), Hyprlang::SConfigOptions{.allowMissingConfig = true});

    config->addConfigValue("ipc", Hyprlang::INT{1L});
    config->addConfigValue("splash", Hyprlang::INT{0L});
    config->addConfigValue("splash_offset", Hyprlang::FLOAT{2.F});
    config->addConfigValue("splash_color", Hyprlang::INT{0x55ffffff});

    config->registerHandler(&handleWallpaper, "wallpaper", {.allowFlags = false});
    config->registerHandler(&handleUnload, "unload", {.allowFlags = false});
    config->registerHandler(&handlePreload, "preload", {.allowFlags = false});
    config->registerHandler(&handleUnloadAll, "unloadAll", {.allowFlags = false});
    config->registerHandler(&handleReload, "reload", {.allowFlags = false});

    config->commence();
}

void CConfigManager::parse() {
    const auto ERROR = config->parse();

    if (ERROR.error)
        std::cout << "Error in config: \n" << ERROR.getError() << "\n";
}

std::string CConfigManager::getMainConfigPath() {
    if (!g_pHyprpaper->m_szExplicitConfigPath.empty())
        return g_pHyprpaper->m_szExplicitConfigPath;

    static const auto paths = Hyprutils::Path::findConfig("hyprpaper");
    if (paths.first.has_value())
        return paths.first.value();
    else
        return "";
}

// trim from both ends
std::string CConfigManager::trimPath(std::string path) {
    if (path.empty())
        return "";

    // trims whitespaces, tabs and new line feeds
    size_t pathStartIndex = path.find_first_not_of(" \t\r\n");
    size_t pathEndIndex   = path.find_last_not_of(" \t\r\n");
    return path.substr(pathStartIndex, pathEndIndex - pathStartIndex + 1);
}
