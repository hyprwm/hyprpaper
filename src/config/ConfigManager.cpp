#include "ConfigManager.hpp"
#include "../Hyprpaper.hpp"

CConfigManager::CConfigManager() {
    // Initialize the configuration
    // Read file from default location
    // or from an explicit location given by user

    std::string configPath = getMainConfigPath();

    std::ifstream ifs;
    ifs.open(configPath);

    if (!ifs.good()) {
        Debug::log(CRIT, "Config file `%s` couldn't be opened.", configPath.c_str());
        exit(1);
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line
            try {
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError += "Config error at line " + std::to_string(linenum) + ": Line parsing error.";
            }

            if (!parseError.empty()) {
                parseError = "Config error at line " + std::to_string(linenum) + ": " + parseError;
                break;
            }

            ++linenum;
        }

        ifs.close();
    }

    if (!parseError.empty()) {
        Debug::log(CRIT, "Exiting because of config parse errors!\n%s", parseError.c_str());
        exit(1);
        return;
    }
}

std::string CConfigManager::getMainConfigPath() {
    if (!g_pHyprpaper->m_szExplicitConfigPath.empty())
        return g_pHyprpaper->m_szExplicitConfigPath;

    static const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    std::string configPath;
    if (!xdgConfigHome)
        configPath = getenv("HOME") + std::string("/.config");
    else
        configPath = xdgConfigHome;

    return configPath + "/hypr/hyprpaper.conf";
}

std::string CConfigManager::removeBeginEndSpacesTabs(std::string str) {
    while (str[0] == ' ' || str[0] == '\t') {
        str = str.substr(1);
    }

    while (str.length() != 0 && (str[str.length() - 1] == ' ' || str[str.length() - 1] == '\t')) {
        str = str.substr(0, str.length() - 1);
    }

    return str;
}

void CConfigManager::parseLine(std::string& line) {
    // first check if its not a comment
    const auto COMMENTSTART = line.find_first_of('#');
    if (COMMENTSTART == 0)
        return;

    // now, cut the comment off
    if (COMMENTSTART != std::string::npos)
        line = line.substr(0, COMMENTSTART);

    // Strip line
    while (line[0] == ' ' || line[0] == '\t') {
        line = line.substr(1);
    }

    // And parse
    // check if command
    const auto EQUALSPLACE = line.find_first_of('=');

    if (EQUALSPLACE == std::string::npos)
        return;

    const auto COMMAND = removeBeginEndSpacesTabs(line.substr(0, EQUALSPLACE));
    const auto VALUE = removeBeginEndSpacesTabs(line.substr(EQUALSPLACE + 1));

    parseKeyword(COMMAND, VALUE);
}

void CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE) {
    if (COMMAND == "wallpaper")
        handleWallpaper(COMMAND, VALUE);
    else if (COMMAND == "preload")
        handlePreload(COMMAND, VALUE);
    else if (COMMAND == "unload")
        handleUnload(COMMAND, VALUE);
    else if (COMMAND == "ipc")
        g_pHyprpaper->m_bIPCEnabled = VALUE == "1" || VALUE == "yes" || VALUE == "on" || VALUE == "true";
    else if (COMMAND == "splash")
        g_pHyprpaper->m_bRenderSplash = VALUE == "1" || VALUE == "yes" || VALUE == "on" || VALUE == "true";
    else if (COMMAND == "splash_offset") {
        try {
            g_pHyprpaper->m_fSplashOffset = std::clamp(std::stof(VALUE), 0.f, 100.f);
        } catch (std::exception& e) {
            parseError = "invalid splash_offset value " + VALUE;
        }
    } else
        parseError = "unknown keyword " + COMMAND;
}

void CConfigManager::handleWallpaper(const std::string& COMMAND, const std::string& VALUE) {
    if (VALUE.find_first_of(',') == std::string::npos) {
        parseError = "wallpaper failed (syntax)";
        return;
    }

    auto MONITOR = VALUE.substr(0, VALUE.find_first_of(','));
    auto WALLPAPER = trimPath(VALUE.substr(VALUE.find_first_of(',') + 1));

    bool contain = false;

    if (WALLPAPER.find("contain:") == 0) {
        WALLPAPER = WALLPAPER.substr(8);
        contain = true;
    }

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    if (!std::filesystem::exists(WALLPAPER)) {
        parseError = "wallpaper failed (no such file)";
        return;
    }

    if (std::find(m_dRequestedPreloads.begin(), m_dRequestedPreloads.end(), WALLPAPER) == m_dRequestedPreloads.end() && !g_pHyprpaper->isPreloaded(WALLPAPER)) {
        parseError = "wallpaper failed (not preloaded)";
        return;
    }

    g_pHyprpaper->clearWallpaperFromMonitor(MONITOR);
    g_pHyprpaper->m_mMonitorActiveWallpapers[MONITOR] = WALLPAPER;
    g_pHyprpaper->m_mMonitorWallpaperRenderData[MONITOR].contain = contain;
}

void CConfigManager::handlePreload(const std::string& COMMAND, const std::string& VALUE) {
    auto WALLPAPER = VALUE;

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    if (!std::filesystem::exists(WALLPAPER)) {
        parseError = "preload failed (no such file)";
        return;
    }

    m_dRequestedPreloads.emplace_back(WALLPAPER);
}

void CConfigManager::handleUnload(const std::string& COMMAND, const std::string& VALUE) {
    auto WALLPAPER = VALUE;

    if (VALUE == "all") {
        handleUnloadAll(COMMAND, VALUE);
        return;
    }

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    g_pHyprpaper->unloadWallpaper(WALLPAPER);
}

void CConfigManager::handleUnloadAll(const std::string& COMMAND, const std::string& VALUE) {
    std::vector<std::string> toUnload;

    for (auto& [name, target] : g_pHyprpaper->m_mWallpaperTargets) {

        bool exists = false;
        for (auto& [mon, target2] : g_pHyprpaper->m_mMonitorActiveWallpaperTargets) {
            if (&target == target2) {
                exists = true;
                break;
            }
        }

        if (exists)
            continue;

        toUnload.emplace_back(name);
    }

    for (auto& tu : toUnload)
        g_pHyprpaper->unloadWallpaper(tu);
}

// trim from both ends
std::string CConfigManager::trimPath(std::string path) {
    // trims whitespaces, tabs and new line feeds
    size_t pathStartIndex = path.find_first_not_of(" \t\r\n");
    size_t pathEndIndex = path.find_last_not_of(" \t\r\n");
    return path.substr(pathStartIndex, pathEndIndex - pathStartIndex + 1);
}
