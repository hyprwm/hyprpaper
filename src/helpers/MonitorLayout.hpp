#pragma once

#include <string>
#include <vector>
#include <optional>

namespace MonitorLayout {
    struct SMonitor {
        std::string name;
        double      x = 0, y = 0; // logical position
        double      w = 0, h = 0; // logical size (transform-adjusted)
        double      scale     = 1.0;
        int         transform = 0;
    };

    // Queries the current monitor layout from Hyprland via IPC.
    // Returns nullopt when not running under Hyprland or on any error.
    std::optional<std::vector<SMonitor>> query();
}
