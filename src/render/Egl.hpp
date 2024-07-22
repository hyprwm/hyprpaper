#pragma once

#include <memory>

#include "Buffer.hpp"

#include <EGL/egl.h>
typedef void* EGLImageKHR;
typedef void* GLeglImageOES;
typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)(EGLDisplay dpy, EGLConfig config, void* native_window, const EGLint* attrib_list);
typedef EGLImageKHR(EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint* attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC)(EGLDisplay dpy, EGLImageKHR image);
typedef void(GL_APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
typedef void(GL_APIENTRYP PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)(GLenum target, GLeglImageOES image);

struct gbm_device;

class CEGL {
  public:
    CEGL(gbm_device*);
    ~CEGL();

    EGLDisplay                                    eglDisplay = nullptr;
    EGLConfig                                     eglConfig  = nullptr;
    EGLContext                                    eglContext = nullptr;

    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC      eglCreatePlatformWindowSurfaceEXT      = nullptr;
    PFNEGLCREATEIMAGEKHRPROC                      eglCreateImageKHR                      = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC                     eglDestroyImageKHR                     = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC           glEGLImageTargetTexture2DOES           = nullptr;
    PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES = nullptr;

    void                                          makeCurrent(EGLSurface surf);
    EGLImageKHR                                   getEglImage(const SDMABUFAttrs& attrs);
    void                                          destroyEglImage(EGLImageKHR image);
};

inline std::unique_ptr<CEGL> g_pEGL;