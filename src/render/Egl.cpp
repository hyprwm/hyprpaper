#include "Egl.hpp"
#include "../debug/Log.hpp"
#include <EGL/eglext.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;

const EGLint                    config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE,
};

const EGLint context_attribs[] = {
    EGL_CONTEXT_MAJOR_VERSION,
    3,
    EGL_CONTEXT_MINOR_VERSION,
    2,
    EGL_NONE,
};

CEGL::CEGL(gbm_device* device) {
    const char* _EXTS = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!_EXTS) {
        if (eglGetError() == EGL_BAD_DISPLAY)
            throw std::runtime_error("EGL_EXT_client_extensions not supported");
        else
            throw std::runtime_error("Failed to query EGL client extensions");
    }

    std::string EXTS = _EXTS;

    if (!EXTS.contains("EGL_EXT_platform_base"))
        throw std::runtime_error("EGL_EXT_platform_base not supported");

    if (!EXTS.contains("EGL_EXT_platform_wayland"))
        throw std::runtime_error("EGL_EXT_platform_wayland not supported");

    eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (eglGetPlatformDisplayEXT == NULL)
        throw std::runtime_error("Failed to get eglGetPlatformDisplayEXT");

    eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
    if (eglCreatePlatformWindowSurfaceEXT == NULL)
        throw std::runtime_error("Failed to get eglCreatePlatformWindowSurfaceEXT");

    eglDisplay     = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, device, NULL);
    EGLint matched = 0;
    if (eglDisplay == EGL_NO_DISPLAY) {
        Debug::log(CRIT, "Failed to create EGL display");
        goto error;
    }

    if (eglInitialize(eglDisplay, NULL, NULL) == EGL_FALSE) {
        Debug::log(CRIT, "Failed to initialize EGL");
        goto error;
    }

    if (!eglChooseConfig(eglDisplay, config_attribs, &eglConfig, 1, &matched)) {
        Debug::log(CRIT, "eglChooseConfig failed");
        goto error;
    }
    if (matched == 0) {
        Debug::log(CRIT, "Failed to match an EGL config");
        goto error;
    }

    eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, context_attribs);
    if (eglContext == EGL_NO_CONTEXT) {
        Debug::log(CRIT, "Failed to create EGL context");
        goto error;
    }

    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (eglDestroyImageKHR == NULL)
        throw std::runtime_error("Failed to get eglDestroyImageKHR");

    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if (eglCreateImageKHR == NULL)
        throw std::runtime_error("Failed to get eglCreateImageKHR");

    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (glEGLImageTargetTexture2DOES == NULL)
        throw std::runtime_error("Failed to get glEGLImageTargetTexture2DOES");

    glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
    if (glEGLImageTargetRenderbufferStorageOES == NULL)
        throw std::runtime_error("Failed to get glEGLImageTargetRenderbufferStorageOES");

    return;

error:
    eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

CEGL::~CEGL() {
    if (eglContext != EGL_NO_CONTEXT)
        eglDestroyContext(eglDisplay, eglContext);

    if (eglDisplay)
        eglTerminate(eglDisplay);

    eglReleaseThread();
}

void CEGL::makeCurrent(EGLSurface surf) {
    eglMakeCurrent(eglDisplay, surf, surf, eglContext);
}

void* CEGL::getEglImage(const SDMABUFAttrs& attrs) {
    std::vector<uint32_t> attribs;

    attribs.push_back(EGL_WIDTH);
    attribs.push_back(attrs.size.x);
    attribs.push_back(EGL_HEIGHT);
    attribs.push_back(attrs.size.y);
    attribs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
    attribs.push_back(attrs.format);

    struct {
        EGLint fd;
        EGLint offset;
        EGLint pitch;
        EGLint modlo;
        EGLint modhi;
    } attrNames[4] = {
        {EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT},
        {EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT}};

    for (int i = 0; i < attrs.planes; i++) {
        attribs.push_back(attrNames[i].fd);
        attribs.push_back(attrs.fds[i]);
        attribs.push_back(attrNames[i].offset);
        attribs.push_back(attrs.offsets[i]);
        attribs.push_back(attrNames[i].pitch);
        attribs.push_back(attrs.strides[i]);
        if (attrs.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs.push_back(attrNames[i].modlo);
            attribs.push_back(attrs.modifier & 0xFFFFFFFF);
            attribs.push_back(attrNames[i].modhi);
            attribs.push_back(attrs.modifier >> 32);
        }
    }

    attribs.push_back(EGL_IMAGE_PRESERVED_KHR);
    attribs.push_back(EGL_TRUE);

    attribs.push_back(EGL_NONE);

    EGLImageKHR image = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, (int*)attribs.data());
    if (image == EGL_NO_IMAGE_KHR) {
        Debug::log(ERR, "EGL: EGLCreateImageKHR failed with 0x%lx", eglGetError());
        return EGL_NO_IMAGE_KHR;
    }

    return image;
}

void CEGL::destroyEglImage(EGLImageKHR image) {
    eglDestroyImageKHR(eglDisplay, image);
}
