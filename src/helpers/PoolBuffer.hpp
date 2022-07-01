#pragma once

#include "../defines.hpp"

struct SPoolBuffer {
    wl_buffer* buffer;
    cairo_surface_t *surface;
    cairo_t *cairo;
    void* data;
    size_t size;
};