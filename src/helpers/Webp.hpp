#pragma once

#include "../defines.hpp"
#include <webp/decode.h>

namespace WEBP {
    cairo_surface_t* createSurfaceFromWEBP(const std::string&);
};
