#include "WallpaperMatcher.hpp"

#include <algorithm>

using namespace std::string_literals;

// Check if a monitor string represents a wildcard (matches all monitors)
// Empty string or "*" are both treated as wildcards.
// "*" is preferred since hyprlang's special category system doesn't properly
// return entries with empty string keys from listKeysForSpecialCategory().
static bool isWildcard(const std::string& monitor) {
    return monitor.empty() || monitor == "*";
}

void CWallpaperMatcher::addState(CConfigManager::SSetting&& s) {
    s.id = ++m_maxId;

    std::erase_if(m_settings, [&s](const auto& e) { return e.monitor == s.monitor; });
    m_settings.emplace_back(std::move(s));
    recalcStates();
}

void CWallpaperMatcher::addStates(std::vector<CConfigManager::SSetting>&& s) {
    for (auto& ss : s) {
        ss.id = ++m_maxId;
    }

    std::erase_if(m_settings, [&s](const auto& e) { return std::ranges::any_of(s, [&e](const auto& el) { return el.monitor == e.monitor; }); });
    m_settings.append_range(std::move(s));
    recalcStates();
}

void CWallpaperMatcher::registerOutput(const std::string_view& s, const std::string_view& desc) {
    m_monitorNames.emplace_back(std::make_pair<>(s, desc));
    recalcStates();
}

void CWallpaperMatcher::unregisterOutput(const std::string_view& s) {
    std::erase_if(m_monitorNames, [&s](const auto& e) { return e.first == s; });
    std::erase_if(m_monitorStates, [&s](const auto& e) { return e.name == s; });
    recalcStates();
}

bool CWallpaperMatcher::outputExists(const std::string_view& s) {
    return std::ranges::any_of(m_monitorNames, [&s](const auto& e) { return e.first == s || "desc:" + e.second == s; });
}

std::optional<CWallpaperMatcher::rw<const CConfigManager::SSetting>> CWallpaperMatcher::getSetting(const std::string_view& monName, const std::string_view& monDesc) {
    for (const auto& m : m_monitorStates) {
        if (m.name != monName)
            continue;

        for (const auto& s : m_settings) {
            if (s.id != m.currentID)
                continue;
            return s;
        }

        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<CWallpaperMatcher::rw<const CConfigManager::SSetting>> CWallpaperMatcher::matchSetting(const std::string_view& monName, const std::string_view& monDesc) {
    // match explicit
    for (const auto& s : m_settings) {
        if (s.monitor != monName && !("desc:"s + std::string{monDesc}).starts_with(s.monitor))
            continue;
        return s;
    }

    // match wildcard (empty string or "*")
    for (const auto& s : m_settings) {
        if (isWildcard(s.monitor))
            return s;
    }

    return std::nullopt;
}

CWallpaperMatcher::SMonitorState& CWallpaperMatcher::getState(const std::string_view& monName) {
    for (auto& s : m_monitorStates) {
        if (s.name == monName)
            return s;
    }

    return m_monitorStates.emplace_back();
}

void CWallpaperMatcher::recalcStates() {
    std::vector<std::string_view> namesChanged;

    for (const auto& [name, desc] : m_monitorNames) {
        const auto STATE       = matchSetting(name, desc);
        auto&      activeState = getState(name);

        if (!STATE)
            activeState = {.name = name, .desc = desc, .currentID = CConfigManager::SETTING_INVALID};
        else {
            activeState.name = name;
            activeState.desc = desc;
            if (activeState.currentID != STATE->get().id) {
                activeState.currentID = STATE->get().id;
                namesChanged.emplace_back(name);
            }
        }
    }

    for (const auto& n : namesChanged) {
        m_events.monitorConfigChanged.emit(n);
    }
}
