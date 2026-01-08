#pragma once

#include <optional>
#include <tuple>

#include "ConfigManager.hpp"

#include <hyprutils/signal/Signal.hpp>

class CWallpaperMatcher {
  public:
    template <typename T>
    using rw = std::reference_wrapper<T>;

    CWallpaperMatcher()  = default;
    ~CWallpaperMatcher() = default;

    CWallpaperMatcher(const CWallpaperMatcher&) = delete;
    CWallpaperMatcher(CWallpaperMatcher&)       = delete;
    CWallpaperMatcher(CWallpaperMatcher&&)      = delete;

    void                                              addState(CConfigManager::SSetting&&);
    void                                              addStates(std::vector<CConfigManager::SSetting>&&);

    void                                              registerOutput(const std::string_view&, const std::string_view&);
    void                                              unregisterOutput(const std::string_view&);
    bool                                              outputExists(const std::string_view&);

    std::optional<rw<const CConfigManager::SSetting>> getSetting(const std::string_view& monName, const std::string_view& monDesc);

    struct {
        Hyprutils::Signal::CSignalT<const std::string_view&> monitorConfigChanged;
    } m_events;

  private:
    void                                              recalcStates();
    std::optional<rw<const CConfigManager::SSetting>> matchSetting(const std::string_view& monName, const std::string_view& monDesc);

    std::vector<CConfigManager::SSetting>             m_settings;

    struct SMonitorState {
        std::string name, desc;
        uint32_t    currentID = CConfigManager::SETTING_INVALID;
    };

    std::vector<std::pair<std::string, std::string>> m_monitorNames;
    std::vector<SMonitorState>                       m_monitorStates;

    uint32_t                                         m_maxId = 0;

    SMonitorState&                                   getState(const std::string_view& monName, const std::string_view& monDesc);
};

inline UP<CWallpaperMatcher> g_matcher = makeUnique<CWallpaperMatcher>();
