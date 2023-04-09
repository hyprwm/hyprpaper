#include "MiscFunctions.hpp"
#include <array>
#include "../debug/Log.hpp"
#include <memory>

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const float& delta) {
    return std::abs(a.x - b.x) < delta && std::abs(a.y - b.y) < delta;
}

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const Vector2D& delta) {
    return std::abs(a.x - b.x) < delta.x && std::abs(a.y - b.y) < delta.y;
}

std::string execAndGet(const char* cmd) {
    std::array<char, 128>                          buffer;
    std::string                                    result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "execAndGet: failed in pipe");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
