#pragma once

#include "../defines.hpp"

struct SPoolBuffer {
    wl_buffer* buffer = nullptr;
    cairo_surface_t* surface = nullptr;
    cairo_t* cairo = nullptr;
    void* data = nullptr;
    size_t size = 0;
};