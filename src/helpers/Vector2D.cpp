#include "Vector2D.hpp"

Vector2D::Vector2D(double xx, double yy) {
    x = xx;
    y = yy;
}

Vector2D::Vector2D() { x = 0; y = 0; }
Vector2D::~Vector2D() = default;

double Vector2D::normalize() {
    // get max abs
    const auto max = abs(x) > abs(y) ? abs(x) : abs(y);

    x /= max;
    y /= max;

    return max;
}

Vector2D Vector2D::floor() const {
    return {static_cast<int>(x), static_cast<int>(y)};
}
