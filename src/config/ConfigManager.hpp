#pragma once
#include "../defines.hpp"

class CConfigManager {
public:
    // gets all the data from the config
    CConfigManager();

    std::deque<std::string> m_dRequestedPreloads;

private:
    std::string parseError = "";

    void parseLine(std::string&);
    std::string removeBeginEndSpacesTabs(std::string in);
    void parseKeyword(const std::string&, const std::string&);

    void handleWallpaper(const std::string&, const std::string&);
    void handlePreload(const std::string&, const std::string&);
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;