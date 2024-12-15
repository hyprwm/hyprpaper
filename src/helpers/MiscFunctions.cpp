#include "MiscFunctions.hpp"
#include <array>
#include "../debug/Log.hpp"
#include <memory>

#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::OS;

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const float& delta) {
    return std::abs(a.x - b.x) < delta && std::abs(a.y - b.y) < delta;
}

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const Vector2D& delta) {
    return std::abs(a.x - b.x) < delta.x && std::abs(a.y - b.y) < delta.y;
}

std::string execAndGet(const char* cmd) {
    CProcess proc("/bin/bash", {"-c", cmd});
    if (!proc.runSync())
        return "";
    return proc.stdOut();
}
