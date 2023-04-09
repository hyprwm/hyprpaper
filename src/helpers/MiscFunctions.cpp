#include "MiscFunctions.hpp"

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const float& delta) {
    return std::abs(a.x - b.x) < delta && std::abs(a.y - b.y) < delta;
}

bool vectorDeltaLessThan(const Vector2D& a, const Vector2D& b, const Vector2D& delta) {
    return std::abs(a.x - b.x) < delta.x && std::abs(a.y - b.y) < delta.y;
}
