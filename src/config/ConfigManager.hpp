#pragma once
#include "../defines.hpp"

class CIPCSocket;

class CConfigManager {
public:
    // gets all the data from the config
    CConfigManager();

    std::deque<std::string> m_dRequestedPreloads;

private:
    std::string parseError;

    void parseLine(std::string&);
    std::string removeBeginEndSpacesTabs(std::string in);
    void parseKeyword(const std::string&, const std::string&);

    void handleWallpaper(const std::string&, const std::string&);
    void handlePreload(const std::string&, const std::string&);
    void handleUnload(const std::string&, const std::string&);
    void handleUnloadAll(const std::string&, const std::string&);
    std::string trim_copy(std::string s);
    void trim(std::string &s);
    std::string ltrim_copy(std::string s);
    void ltrim(std::string &s);
    std::string rtrim_copy(std::string s);
    void rtrim(std::string &s);

    friend class CIPCSocket;
};

inline std::unique_ptr<CConfigManager> g_pConfigManager;
