#pragma once

#include "../defines.hpp"
#include <jpeglib.h>

namespace JPEG {
    cairo_surface_t* createSurfaceFromJPEG(const std::string&);
};