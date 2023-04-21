#include "ConfigManager.hpp"
#include "../Hyprpaper.hpp"
#include "../debug/Log.hpp"

#include <algorithm>
#include <string_view>
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
            Debug::log(CRIT, "No config file provided. Specified file `%s` couldn't be opened.", configPath.c_str());
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
    else if (COMMAND == "splash")
        g_pHyprpaper->m_bRenderSplash = VALUE == "1" || VALUE == "yes" || VALUE == "on" || VALUE == "true";
    else
        parseError = "unknown keyword " + COMMAND;
}
namespace {
    inline static std::size_t skip_intersection_chars(std::string_view src, std::string_view test_chars) {
        auto l_test_chars = test_chars.size();
        decltype(src.size()) i = 0;

        decltype(src.size()) match_index = 0;
        while (i < src.size()) {
            if (src[i] == test_chars[match_index]) {
                i++;
                continue;
            }
            match_index++;
            decltype(i) j = 0;
            while (match_index < l_test_chars && src[i] != test_chars[match_index]) {
                match_index++;
                j++;
            }
            if (match_index < l_test_chars)
                continue;
            while (j < l_test_chars && src[i] != test_chars[match_index - l_test_chars]) {
                match_index++;
                j++;
            }
            if (j == l_test_chars)
                break;
            match_index = match_index % l_test_chars;
        }
        return i;
    }
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
        auto recover = [&, this]() -> bool {
            bool try_next = false;

            auto count = skip_intersection_chars(WALLPAPER, " \t\r\n");
            std::string_view sv(WALLPAPER.begin() + count, WALLPAPER.end());
            if (sv.starts_with("next")) {
                count += 4;
                if (skip_intersection_chars({sv.begin() + 4, sv.end()}, " \t\r\n") + count == WALLPAPER.size()) {
                    try_next = true;
                }
            }

            if (!try_next) {
                return false;
            }

            // todo how to get that which monitor the current window associate with
            // try default monitor name
            bool default_name = false;
            if (skip_intersection_chars(MONITOR, " \t\r\n") == MONITOR.size()) {
                if (g_pHyprpaper->m_vMonitors.empty() || !g_pHyprpaper->m_mMonitorActiveWallpaperTargets.contains(g_pHyprpaper->m_vMonitors[0].get())) {
                    return false;
                }
                default_name = true;
                MONITOR = "";
            } else {
                int monitori = -1;
                for (int i = 0; auto&& p : g_pHyprpaper->m_vMonitors) {
                    if (p->name == MONITOR) {
                        monitori = i;
                        break;
                    }
                    i++;
                }

                if (monitori == -1) {
                    return false;
                }

                if (monitori == 0) {
                    default_name = true;
                    MONITOR = "";
                }
            }

            auto&& s = g_pHyprpaper->m_mMonitorActiveWallpapers[MONITOR];

            auto it = g_pHyprpaper->m_mWallpaperTargets.find(s);

            if (it != g_pHyprpaper->m_mWallpaperTargets.end()) {
                it++;
            } else {
                return false;
            }

            if (it == g_pHyprpaper->m_mWallpaperTargets.end()) {
                it = g_pHyprpaper->m_mWallpaperTargets.begin();
            }
            if (default_name) {
                auto&& dn = g_pHyprpaper->m_vMonitors[0]->name;
                g_pHyprpaper->clearWallpaperFromMonitor(dn);
                g_pHyprpaper->m_mMonitorActiveWallpapers[dn] = it->first;
                g_pHyprpaper->m_mMonitorWallpaperRenderData[dn].contain = contain;
            }
            g_pHyprpaper->clearWallpaperFromMonitor(MONITOR);
            g_pHyprpaper->m_mMonitorActiveWallpapers[MONITOR] = it->first;
            g_pHyprpaper->m_mMonitorWallpaperRenderData[MONITOR].contain = contain;

            return true;
        };
        if (!recover()) {
            parseError = "wallpaper failed (no such file)";
            return;
        }
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
