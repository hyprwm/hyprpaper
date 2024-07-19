#include "Renderer.hpp"
#include "protocols/linux-dmabuf-v1.hpp"
#include "protocols/wlr-layer-shell-unstable-v1.hpp"
#include "protocols/wayland.hpp"
#include "protocols/viewporter.hpp"
#include "../Hyprpaper.hpp"
#include <xf86drm.h>
#include "Buffer.hpp"
#include "LayerSurface.hpp"
#include "Egl.hpp"
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2.h>
#include <hyprutils/math/Box.hpp>
#include "Math.hpp"
using namespace Hyprutils::Math;

// ------------------- shader utils

GLuint compileShader(const GLuint& type, std::string src) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (ok == GL_FALSE)
        return 0;

    return shader;
}

GLuint createProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert);
    if (vertCompiled == 0)
        return 0;

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag);
    if (fragCompiled == 0)
        return 0;

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
        return 0;

    return prog;
}

inline const std::string VERT_SRC = R"#(
uniform mat3 proj;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = texcoord;
})#";

inline const std::string FRAG_SRC = R"#(
precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

void main() {
    gl_FragColor = texture2D(tex, v_texcoord);
})#";

inline const std::string FRAG_SRC_EXT = R"#(
#extension GL_OES_EGL_image_external : require
precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform samplerExternalOES texture0;

void main() {
    gl_FragColor = texture2D(texture0, v_texcoord);
})#";

// -------------------

CRenderer::CRenderer(bool gpu) {
    if (gpu) {
        dmabufFeedback = makeShared<CCZwpLinuxDmabufFeedbackV1>(g_pHyprpaper->m_pLinuxDmabuf->sendGetDefaultFeedback());

        dmabufFeedback->setMainDevice([this](CCZwpLinuxDmabufFeedbackV1* r, wl_array* deviceArr) {
            dev_t device;
            if (deviceArr->size != sizeof(device))
                abort();
            memcpy(&device, deviceArr->data, sizeof(device));

            drmDevice* drmDev;
            if (drmGetDeviceFromDevId(device, /* flags */ 0, &drmDev) != 0) {
                Debug::log(ERR, "zwp_linux_dmabuf_v1: drmGetDeviceFromDevId failed");
                return;
            }

            const char* name = nullptr;
            if (drmDev->available_nodes & (1 << DRM_NODE_RENDER))
                name = drmDev->nodes[DRM_NODE_RENDER];
            else {
                // Likely a split display/render setup. Pick the primary node and hope
                // Mesa will open the right render node under-the-hood.
                if (!(drmDev->available_nodes & (1 << DRM_NODE_PRIMARY)))
                    abort();
                name = drmDev->nodes[DRM_NODE_PRIMARY];
                Debug::log(WARN, "zwp_linux_dmabuf_v1: DRM device has no render node, using primary");
            }

            if (!name) {
                Debug::log(ERR, "zwp_linux_dmabuf_v1: no node name");
                return;
            }

            nodeName = name;

            drmFreeDevice(&drmDev);

            Debug::log(LOG, "zwp_linux_dmabuf_v1: got node %s", nodeName.c_str());
        });

        wl_display_roundtrip(g_pHyprpaper->m_sDisplay);

        if (!nodeName.empty()) {
            nodeFD = open(nodeName.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (nodeFD < 0) {
                Debug::log(ERR, "zwp_linux_dmabuf_v1: failed to open node");
                return;
            }

            Debug::log(LOG, "zwp_linux_dmabuf_v1: opened node %s with fd %i", nodeName.c_str(), nodeFD);

            gbmDevice = gbm_create_device(nodeFD);
        }

        g_pEGL = std::make_unique<CEGL>(gbmDevice);
        g_pEGL->makeCurrent(EGL_NO_SURFACE);

        gl.shader.program = createProgram(VERT_SRC, FRAG_SRC);
        if (gl.shader.program == 0) {
            Debug::log(ERR, "renderer: failed to link shader");
            return;
        }

        gl.shader.proj      = glGetUniformLocation(gl.shader.program, "proj");
        gl.shader.posAttrib = glGetAttribLocation(gl.shader.program, "pos");
        gl.shader.texAttrib = glGetAttribLocation(gl.shader.program, "texcoord");
        gl.shader.tex       = glGetUniformLocation(gl.shader.program, "tex");
    }
}

void CRenderer::renderWallpaperForMonitor(SMonitor* pMonitor) {

    const auto PBUFFER = getOrCreateBuffer(pMonitor);

    if (!g_pHyprpaper->m_mMonitorActiveWallpaperTargets[pMonitor])
        g_pHyprpaper->recheckMonitor(pMonitor);

    if (gbmDevice)
        renderGpu(pMonitor, PBUFFER);
    else
        renderCpu(pMonitor, PBUFFER);

    if (pMonitor->pCurrentLayerSurface) {
        pMonitor->pCurrentLayerSurface->pSurface->sendAttach(PBUFFER->buffer.get(), 0, 0);
        pMonitor->pCurrentLayerSurface->pSurface->sendSetBufferScale(pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? 1 : pMonitor->scale);
        pMonitor->pCurrentLayerSurface->pSurface->sendDamageBuffer(0, 0, 0xFFFF, 0xFFFF);

        // our wps are always opaque
        auto opaqueRegion = makeShared<CCWlRegion>(g_pHyprpaper->m_pCompositor->sendCreateRegion());
        opaqueRegion->sendAdd(0, 0, PBUFFER->pixelSize.x, PBUFFER->pixelSize.y);
        pMonitor->pCurrentLayerSurface->pSurface->sendSetOpaqueRegion(opaqueRegion.get());

        if (pMonitor->pCurrentLayerSurface->pFractionalScaleInfo) {
            Debug::log(LOG, "Submitting viewport dest size %ix%i for %x", static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)),
                       pMonitor->pCurrentLayerSurface);
            pMonitor->pCurrentLayerSurface->pViewport->sendSetDestination(static_cast<int>(std::round(pMonitor->size.x)), static_cast<int>(std::round(pMonitor->size.y)));
        }
        pMonitor->pCurrentLayerSurface->pSurface->sendCommit();
    }

    // check if we dont need to remove a wallpaper
    if (pMonitor->layerSurfaces.size() > 1) {
        for (auto it = pMonitor->layerSurfaces.begin(); it != pMonitor->layerSurfaces.end(); it++) {
            if (pMonitor->pCurrentLayerSurface != it->get()) {
                pMonitor->layerSurfaces.erase(it);
                break;
            }
        }
    }
}

SP<CBuffer> CRenderer::getOrCreateBuffer(SMonitor* pMonitor) {
    Vector2D size = pMonitor->size;

    if (pMonitor->pCurrentLayerSurface)
        size = size * pMonitor->pCurrentLayerSurface->fScale;

    if (monitorBuffers.contains(pMonitor) && monitorBuffers.at(pMonitor)->pixelSize == size)
        return monitorBuffers.at(pMonitor);

    auto buf = makeShared<CBuffer>(size);

    monitorBuffers[pMonitor] = buf;

    return buf;
}

void CRenderer::renderCpu(SMonitor* pMonitor, SP<CBuffer> buf) {
    renderWallpaper(pMonitor, buf->cpu.surface, buf->cpu.cairo);
}

SGLTex CRenderer::glTex(void* cairoData, const Vector2D& size) {
    SGLTex tex;

    glGenTextures(1, &tex.texid);

    glBindTexture(GL_TEXTURE_2D, tex.texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, (size.x * 4) / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, cairoData);
    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

inline const float fullVerts[] = {
    1, 0, // top right
    0, 0, // top left
    1, 1, // bottom right
    0, 1, // bottom left
};

void CRenderer::renderGpu(SMonitor* pMonitor, SP<CBuffer> buf) {
    // TODO: get the image in 10b
    // TODO: actually render with the gpu

    auto pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, buf->pixelSize.x, buf->pixelSize.y);
    auto pCairo        = cairo_create(pCairoSurface);

    renderWallpaper(pMonitor, pCairoSurface, pCairo, true);

    cairo_destroy(pCairo);

    g_pEGL->makeCurrent(EGL_NO_SURFACE);

    SGLTex fromTex = glTex(cairo_image_surface_get_data(pCairoSurface), buf->pixelSize);

    GLuint rboID = 0, fboID = 0;

    glGenRenderbuffers(1, &rboID);
    glBindRenderbuffer(GL_RENDERBUFFER, rboID);
    g_pEGL->glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, (GLeglImageOES)buf->gpu.eglImage);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboID);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Debug::log(ERR, "EGL: failed to create a rbo");
        return;
    }

    glClearColor(0.77F, 0.F, 0.74F, 1.F);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // done, let's render the texture to the rbo
    // first, render the bg
    double         scale = 1.0;
    Vector2D       origin;

    const auto     PWALLPAPERTARGET = g_pHyprpaper->m_mMonitorActiveWallpaperTargets[pMonitor];
    const auto     CONTAIN          = g_pHyprpaper->m_mMonitorWallpaperRenderData[pMonitor->name].contain;
    const double   SURFACESCALE = pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale;
    const Vector2D DIMENSIONS   = Vector2D{std::round(pMonitor->size.x * SURFACESCALE), std::round(pMonitor->size.y * SURFACESCALE)};

    const bool     LOWASPECTRATIO = pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y;
    if ((CONTAIN && !LOWASPECTRATIO) || (!CONTAIN && LOWASPECTRATIO)) {
        scale    = DIMENSIONS.x / PWALLPAPERTARGET->m_vSize.x;
        origin.y = -(PWALLPAPERTARGET->m_vSize.y * scale - DIMENSIONS.y) / 2.0 / scale;
    } else {
        scale    = DIMENSIONS.y / PWALLPAPERTARGET->m_vSize.y;
        origin.x = -(PWALLPAPERTARGET->m_vSize.x * scale - DIMENSIONS.x) / 2.0 / scale;
    }

    renderTexture(PWALLPAPERTARGET->gpu.textureID, CBox{origin, PWALLPAPERTARGET->m_vSize * scale}, buf->pixelSize);

    // then, any decoration we got from cairo
    renderTexture(fromTex.texid, {{}, buf->pixelSize}, buf->pixelSize);

    // rendered, cleanup

    glFlush();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glDeleteFramebuffers(1, &fboID);
    glDeleteRenderbuffers(1, &rboID);
    glDeleteTextures(1, &fromTex.texid);
    cairo_surface_destroy(pCairoSurface);
}

void CRenderer::renderTexture(GLuint texid, const CBox& box, const Vector2D& viewport) {
    CBox  renderBox = {{}, viewport};

    float mtx[9];
    float base[9];
    float monitorProj[9];
    matrixIdentity(base);

    auto& SHADER = gl.shader;

    // KMS uses flipped y, we have to do FLIPPED_180
    matrixTranslate(base, viewport.x / 2.0, viewport.y / 2.0);
    matrixTransform(base, HYPRUTILS_TRANSFORM_FLIPPED_180);
    matrixTranslate(base, -viewport.x / 2.0, -viewport.y / 2.0);

    projectBox(mtx, renderBox, HYPRUTILS_TRANSFORM_FLIPPED_180, 0, base);

    matrixProjection(monitorProj, viewport.x, viewport.y, HYPRUTILS_TRANSFORM_FLIPPED_180);

    float glMtx[9];
    matrixMultiply(glMtx, monitorProj, mtx);

    glViewport(0, 0, viewport.x, viewport.y);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texid);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glUseProgram(SHADER.program);

    glDisable(GL_SCISSOR_TEST);

    matrixTranspose(glMtx, glMtx);
    glUniformMatrix3fv(SHADER.proj, 1, GL_FALSE, glMtx);

    glUniform1i(SHADER.tex, 0);

    glVertexAttribPointer(SHADER.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(SHADER.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(SHADER.posAttrib);
    glEnableVertexAttribArray(SHADER.texAttrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(SHADER.posAttrib);
    glDisableVertexAttribArray(SHADER.texAttrib);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void CRenderer::renderWallpaper(SMonitor* pMonitor, cairo_surface_t* pCairoSurface, cairo_t* pCairo, bool splashOnly) {
    static auto* const PRENDERSPLASH = reinterpret_cast<Hyprlang::INT* const*>(g_pConfigManager->config->getConfigValuePtr("splash")->getDataStaticPtr());
    static auto* const PSPLASHOFFSET = reinterpret_cast<Hyprlang::FLOAT* const*>(g_pConfigManager->config->getConfigValuePtr("splash_offset")->getDataStaticPtr());

    const auto         PWALLPAPERTARGET = g_pHyprpaper->m_mMonitorActiveWallpaperTargets[pMonitor];
    const auto         CONTAIN          = g_pHyprpaper->m_mMonitorWallpaperRenderData[pMonitor->name].contain;

    if (!PWALLPAPERTARGET) {
        Debug::log(CRIT, "wallpaper target null in render??");
        exit(1);
    }

    const double   SURFACESCALE = pMonitor->pCurrentLayerSurface && pMonitor->pCurrentLayerSurface->pFractionalScaleInfo ? pMonitor->pCurrentLayerSurface->fScale : pMonitor->scale;
    const Vector2D DIMENSIONS   = Vector2D{std::round(pMonitor->size.x * SURFACESCALE), std::round(pMonitor->size.y * SURFACESCALE)};

    cairo_save(pCairo);
    cairo_set_source_rgba(pCairo, 0, 0, 0, 0);
    cairo_set_operator(pCairo, CAIRO_OPERATOR_SOURCE);
    cairo_paint(pCairo);
    cairo_restore(pCairo);

    // always draw a black background behind the wallpaper
    if (!splashOnly) {
        cairo_set_source_rgb(pCairo, 0, 0, 0);
        cairo_rectangle(pCairo, 0, 0, DIMENSIONS.x, DIMENSIONS.y);
        cairo_fill(pCairo);
        cairo_surface_flush(pCairoSurface);
    }

    // get scale
    double     scale;
    Vector2D   origin;

    const bool LOWASPECTRATIO = pMonitor->size.x / pMonitor->size.y > PWALLPAPERTARGET->m_vSize.x / PWALLPAPERTARGET->m_vSize.y;
    if ((CONTAIN && !LOWASPECTRATIO) || (!CONTAIN && LOWASPECTRATIO)) {
        scale    = DIMENSIONS.x / PWALLPAPERTARGET->m_vSize.x;
        origin.y = -(PWALLPAPERTARGET->m_vSize.y * scale - DIMENSIONS.y) / 2.0 / scale;
    } else {
        scale    = DIMENSIONS.y / PWALLPAPERTARGET->m_vSize.y;
        origin.x = -(PWALLPAPERTARGET->m_vSize.x * scale - DIMENSIONS.x) / 2.0 / scale;
    }

    cairo_scale(pCairo, scale, scale);

    if (!splashOnly) {
        Debug::log(LOG, "Image data for %s: %s at [%.2f, %.2f], scale: %.2f (original image size: [%i, %i])", pMonitor->name.c_str(), PWALLPAPERTARGET->m_szPath.c_str(), origin.x,
                   origin.y, scale, (int)PWALLPAPERTARGET->m_vSize.x, (int)PWALLPAPERTARGET->m_vSize.y);

        cairo_set_source_surface(pCairo, PWALLPAPERTARGET->cpu.cairoSurface, origin.x, origin.y);

        cairo_paint(pCairo);
    }

    if (**PRENDERSPLASH && getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        auto SPLASH = execAndGet("hyprctl splash");
        SPLASH.pop_back();

        Debug::log(LOG, "Rendering splash: %s", SPLASH.c_str());

        cairo_select_font_face(pCairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        const auto FONTSIZE = (int)(DIMENSIONS.y / 76.0 / scale);
        cairo_set_font_size(pCairo, FONTSIZE);

        static auto* const PSPLASHCOLOR = reinterpret_cast<Hyprlang::INT* const*>(g_pConfigManager->config->getConfigValuePtr("splash_color")->getDataStaticPtr());

        Debug::log(LOG, "Splash color: %x", **PSPLASHCOLOR);

        cairo_set_source_rgba(pCairo, ((**PSPLASHCOLOR >> 16) & 0xFF) / 255.0, ((**PSPLASHCOLOR >> 8) & 0xFF) / 255.0, (**PSPLASHCOLOR & 0xFF) / 255.0,
                              ((**PSPLASHCOLOR >> 24) & 0xFF) / 255.0);

        cairo_text_extents_t textExtents;
        cairo_text_extents(pCairo, SPLASH.c_str(), &textExtents);

        cairo_move_to(pCairo, ((DIMENSIONS.x - textExtents.width * scale) / 2.0) / scale, ((DIMENSIONS.y * (100 - **PSPLASHOFFSET)) / 100 - textExtents.height * scale) / scale);

        Debug::log(LOG, "Splash font size: %d, pos: %.2f, %.2f", FONTSIZE, (DIMENSIONS.x - textExtents.width) / 2.0 / scale,
                   ((DIMENSIONS.y * (100 - **PSPLASHOFFSET)) / 100 - textExtents.height * scale) / scale);

        cairo_show_text(pCairo, SPLASH.c_str());
    }

    cairo_restore(pCairo);
    cairo_surface_flush(pCairoSurface);
}