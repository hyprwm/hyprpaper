#include "ConfigManager.hpp"
#include "../Hyprpaper.hpp"

CConfigManager::CConfigManager() {
    // init the entire thing

    const char* const ENVHOME = getenv("HOME");
    const std::string CONFIGPATH = ENVHOME + (std::string) "/.config/hypr/hyprpaper.conf";

    std::ifstream ifs;
    ifs.open(CONFIGPATH);

    if (!ifs.good()) {
        Debug::log(CRIT, "Hyprpaper was not provided a config!");
        exit(1);
        return; //jic
    }

    std::string line = "";
    int linenum = 1;
    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            // Read line by line.
            try {
                parseLine(line);
            } catch (...) {
                Debug::log(ERR, "Error reading line from config. Line:");
                Debug::log(NONE, "%s", line.c_str());

                parseError += "Config error at line " + std::to_string(linenum) + ": Line parsing error.";
            }

            if (parseError != "" && parseError.find("Config error at line") != 0) {
                parseError = "Config error at line " + std::to_string(linenum) + ": " + parseError;
            }

            ++linenum;
        }

        ifs.close();
    }

    if (parseError != "") {
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

    // remove shit at the beginning
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
    //

    parseKeyword(COMMAND, VALUE);
}

void CConfigManager::parseKeyword(const std::string& COMMAND, const std::string& VALUE) {
    if (COMMAND == "wallpaper")
        handleWallpaper(COMMAND, VALUE);
    else if (COMMAND == "preload")
        handlePreload(COMMAND, VALUE);
    else if (COMMAND == "unload")
        handleUnload(COMMAND, VALUE);
    else
        parseError = "unknown keyword " + COMMAND;
}

void CConfigManager::handleWallpaper(const std::string& COMMAND, const std::string& VALUE) {
    if (VALUE.find_first_of(',') == std::string::npos) {
        parseError = "wallpaper failed (syntax)";
        return;
    }

    auto MONITOR = VALUE.substr(0, VALUE.find_first_of(','));
    auto WALLPAPER = VALUE.substr(VALUE.find_first_of(',') + 1);

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

    if (WALLPAPER[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        WALLPAPER = std::string(ENVHOME) + WALLPAPER.substr(1);
    }

    g_pHyprpaper->unloadWallpaper(WALLPAPER);
}
