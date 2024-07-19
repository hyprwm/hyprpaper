#pragma once

#include "../defines.hpp"
#include <EGL/egl.h>
#include <gbm.h>

class CCWlBuffer;

struct SDMABUFAttrs {
    bool                      success = false;
    Hyprutils::Math::Vector2D size;
    uint32_t                  format   = 0; // fourcc
    uint64_t                  modifier = 0;

    int                       planes  = 1;
    std::array<uint32_t, 4>   offsets = {0};
    std::array<uint32_t, 4>   strides = {0};
    std::array<int, 4>        fds     = {-1, -1, -1, -1};
};

class CBuffer {
  public:
    CBuffer(const Vector2D& size);
    ~CBuffer();

    struct {
        cairo_surface_t* surface = nullptr;
        cairo_t*         cairo   = nullptr;
        void*            data    = nullptr;
        size_t           size    = 0;
        std::string      name    = "";
    } cpu;

    struct {
        void*        eglImage = nullptr;
        gbm_bo*      bo       = nullptr;
        SDMABUFAttrs attrs;
    } gpu;

    SP<CCWlBuffer> buffer = nullptr;

    std::string    target = "";
    Vector2D       pixelSize;

  private:
    void destroyPool();
    void createPool();

    void createGpu();
    void destroyGpu();
};