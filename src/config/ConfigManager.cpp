#include "ConfigManager.hpp"
#include "../Hyprpaper.hpp"

CConfigManager::CConfigManager() {
    // Initialize the configuration
    // Read file from default location
    // or from an explicit location given by user

    std::string configPath;
    if (g_pHyprpaper->m_szExplicitConfigPath.empty()) {
        const char* const ENVHOME = getenv("HOME");
        configPath = ENVHOME + std::string("/.config/hypr/hyprpaper.conf");
    } else {
        configPath = g_pHyprpaper->m_szExplicitConfigPath;
    }

    std::ifstream ifs;
    ifs.open(configPath);

    if (!ifs.good()) {
        if (g_pHyprpaper->m_szExplicitConfigPath.empty()) {
            Debug::log(CRIT, "No config file provided. Default config file `~/.config/hypr/hyprpaper.conf` couldn't be opened.");
        } else {
            Debug::log(CRIT, "No config file provided. Specified file `%s` couldn't be opened.", configPath);
        }
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
    else
        parseError = "unknown keyword " + COMMAND;
}

void CConfigManager::handleWallpaper(const std::string& COMMAND, const std::string& VALUE) {
    if (VALUE.find_first_of(',') == std::string::npos) {
        parseError = "wallpaper failed (syntax)";
        return;
    }

    auto MONITOR = VALUE.substr(0, VALUE.find_first_of(','));
    auto WALLPAPER = trim_copy(VALUE.substr(VALUE.find_first_of(',') + 1));

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

    for (auto&[name, target] : g_pHyprpaper->m_mWallpaperTargets) {

        bool exists = false;
        for (auto&[mon, target2] : g_pHyprpaper->m_mMonitorActiveWallpaperTargets) {
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


// trim from start (in place)
void CConfigManager::ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void CConfigManager::rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void CConfigManager::trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// trim from start (copying)
std::string CConfigManager::ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
std::string CConfigManager::rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
std::string CConfigManager::trim_copy(std::string s) {
    trim(s);
    return s;
}