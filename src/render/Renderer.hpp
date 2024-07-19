#pragma once

#include "../defines.hpp"
#include <gbm.h>
#include <unordered_map>
#include <hyprutils/math/Box.hpp>
using namespace Hyprutils::Math;

struct SMonitor;
class CBuffer;
class CCZwpLinuxDmabufFeedbackV1;

struct SGLTex {
    void*  image  = nullptr;
    GLuint texid  = 0;
    GLuint target = GL_TEXTURE_2D;
};

class CRenderer {
  public:
    CRenderer(bool gpu);

    gbm_device* gbmDevice = nullptr;
    std::string nodeName  = "";
    int         nodeFD    = -1;

    void        renderWallpaperForMonitor(SMonitor* pMonitor);
    SGLTex      glTex(void* cairoData, const Vector2D& size);

    // no need for double-buffering as we don't update the images
    std::unordered_map<SMonitor*, SP<CBuffer>> monitorBuffers;

  private:
    SP<CCZwpLinuxDmabufFeedbackV1> dmabufFeedback;

    void                           renderCpu(SMonitor* pMonitor, SP<CBuffer> buf);
    void                           renderGpu(SMonitor* pMonitor, SP<CBuffer> buf);

    void                           renderWallpaper(SMonitor* pMonitor, cairo_surface_t* pCairoSurface, cairo_t* pCairo, bool splashOnly = false);
    void                           renderTexture(GLuint texid, const CBox& box, const Vector2D& viewport);

    SP<CBuffer>                    getOrCreateBuffer(SMonitor* pMonitor);

    struct {
        struct SShader {
            GLuint program = 0;
            GLint  proj = -1, tex = -1, posAttrib = -1, texAttrib = -1;
        } shader;
    } gl;
};

inline std::unique_ptr<CRenderer> g_pRenderer;
