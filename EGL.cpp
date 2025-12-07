#include "EGL.h"
#include <cmath>
#include <vector>
#include "Context.h"
#include "ABCOS.h"

#define LIBRARY_NAME "EGL"

static EGLint lastError = EGL_SUCCESS;

//no more double buffering
struct Surface
{
    sfg::Display* display;
    std::pair<uint16_t*, uint16_t*>* pixels = nullptr;
    uint16_t* width = nullptr;
    uint16_t* height = nullptr;
    bool firstFrameBuffer = true;
    bool inited = false;
    Surface()
    {
    }
    void init(uint16_t width, uint16_t height)
    {
        if (inited)
            return;
        if (width == 0 || height == 0)
            return;
        inited = true;
        display = OS::InitDisplay();
        *this->width = width;
        *this->height = height;
    }
    ~Surface()
    {
    }
    void swapBuffers()
    {
        if (!pixels)
            return;
        OS::Draw();
        display->display();
        OS::FrameStart();
    }
};

struct ABCv2_Display
{
    std::unique_ptr<Surface> surface = nullptr;
    std::unique_ptr<Context> context = nullptr;
    std::vector<Config> configs = {
        {5, 6, 5, 8, 0, 0, 0, 0, EGL_NONE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_OPENGL_ES_BIT },
        {5, 6, 5, 8, 16, 0, 0, 0, EGL_NONE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_OPENGL_ES_BIT },
        {5, 6, 5, 8, 16, 8, 0, 0, EGL_NONE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_OPENGL_ES_BIT },
        {5, 6, 5, 8, 0, 8, 0, 0, EGL_NONE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_OPENGL_ES_BIT },
    };
};

extern Context* context;
ABCv2_Display eglABCv2;

void ESPCreateWindow(const uint16_t width, const uint16_t height)
{
    eglABCv2.surface->init(width, height);
    eglABCv2.context->pixels.first = eglABCv2.surface->display->getBuffer();
    eglABCv2.context->pixels.first = eglABCv2.surface->display->getBuffer();
    eglABCv2.context->createDepthBuffer();
    eglABCv2.context->createStencilBuffer();
    eglABCv2.context->createAlphaBuffer();
    eglABCv2.context->initViewport();
}

EGLBoolean eglBindAPI(EGLenum api)
{
    ESP_LOGE(LIBRARY_NAME, "eglBindAPI unimplemented");
    return false;
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    ESP_LOGE(LIBRARY_NAME, "eglBindTexImage unimplemented");
    return false;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size,
    EGLint* num_config)
{
    ESP_LOGE(LIBRARY_NAME, "eglChooseConfig unimplemented");
    return false;
}

EGLint eglClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout)
{
    ESP_LOGE(LIBRARY_NAME, "eglClientWaitSync unimplemented");
    return false;
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
    ESP_LOGE(LIBRARY_NAME, "eglCopyBuffers unimplemented");
    return false;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context,
    const EGLint* attrib_list)
{
    if (dpy == nullptr)
        return EGL_NO_CONTEXT;
    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    if (!display->context)
        display->context = std::make_unique<Context>(config);
    return display->context.get();
}

EGLImage eglCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer,
    const EGLAttrib* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreateImage unimplemented");
    return nullptr;
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
    EGLConfig config, const EGLint* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreatePbufferFromClientBuffer unimplemented");
    return nullptr;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list)
{
    if (dpy == nullptr)
        return EGL_NO_SURFACE;
    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    if (!display->surface)
        display->surface = std::make_unique<Surface>();
    return display->surface.get();
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap,
    const EGLint* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreatePixmapSurface unimplemented");
    return nullptr;
}

EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay dpy, EGLConfig config, void* native_pixmap,
    const EGLAttrib* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreatePlatformPixmapSurface unimplemented");
    return nullptr;
}

EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config, void* native_window,
    const EGLAttrib* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreatePlatformWindowSurface unimplemented");
    return nullptr;
}

EGLSync eglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglCreateSync unimplemented");
    return nullptr;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
    const EGLint* attrib_list)
{
    if (dpy == nullptr)
        return EGL_NO_SURFACE;

    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    if (!display->surface)
        display->surface = std::make_unique<Surface>();
    return display->surface.get();
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    if (display->context)
        display->context.reset();
    lastError = EGL_BAD_CONTEXT;
    return EGL_FALSE;
}

EGLBoolean eglDestroyImage(EGLDisplay dpy, EGLImage image)
{
    ESP_LOGE(LIBRARY_NAME, "eglDestroyImage unimplemented");
    return false;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    ESP_LOGE(LIBRARY_NAME, "eglDestroySurface unimplemented");
    return false;
}

EGLBoolean eglDestroySync(EGLDisplay dpy, EGLSync sync)
{
    ESP_LOGE(LIBRARY_NAME, "eglDestroySync unimplemented");
    return false;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (value == nullptr)
        return EGL_FALSE;
    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    for (auto& i : display->configs)
        if (&i == config)
        {
            Config* con = &i;
            switch (attribute)
            {
            case EGL_ALPHA_SIZE: *value = con->alpha; return EGL_TRUE;
            case EGL_ALPHA_MASK_SIZE: ESP_LOGE(LIBRARY_NAME, "EGL_ALPHA_MASK_SIZE not implemented"); return EGL_FALSE;
            case EGL_BIND_TO_TEXTURE_RGB: ESP_LOGE(LIBRARY_NAME, "EGL_BIND_TO_TEXTURE_RGB not implemented"); return EGL_FALSE;
            case EGL_BIND_TO_TEXTURE_RGBA: ESP_LOGE(LIBRARY_NAME, "EGL_BIND_TO_TEXTURE_RGBA not implemented"); return EGL_FALSE;
            case EGL_BLUE_SIZE: *value = con->blue; return EGL_TRUE;
            case EGL_BUFFER_SIZE: ESP_LOGE(LIBRARY_NAME, "EGL_BUFFER_SIZE not implemented"); return EGL_FALSE;
            case EGL_COLOR_BUFFER_TYPE: ESP_LOGE(LIBRARY_NAME, "EGL_COLOR_BUFFER_TYPE not implemented"); return EGL_FALSE;
            case EGL_CONFIG_CAVEAT: *value = con->caveat; return EGL_TRUE;
            case EGL_CONFIG_ID: ESP_LOGE(LIBRARY_NAME, "EGL_CONFIG_ID not implemented"); return EGL_FALSE;
            case EGL_CONFORMANT: ESP_LOGE(LIBRARY_NAME, "EGL_CONFORMANT not implemented"); return EGL_FALSE;
            case EGL_DEPTH_SIZE: *value = con->depth; return EGL_TRUE;
            case EGL_GREEN_SIZE: *value = con->green; return EGL_TRUE;
            case EGL_LEVEL: ESP_LOGE(LIBRARY_NAME, "EGL_LEVEL not implemented"); return EGL_FALSE;
            case EGL_LUMINANCE_SIZE: ESP_LOGE(LIBRARY_NAME, "EGL_LUMINANCE_SIZE not implemented"); return EGL_FALSE;
            case EGL_MAX_PBUFFER_WIDTH: ESP_LOGE(LIBRARY_NAME, "EGL_MAX_PBUFFER_WIDTH not implemented"); return EGL_FALSE;
            case EGL_MAX_PBUFFER_HEIGHT: ESP_LOGE(LIBRARY_NAME, "EGL_MAX_PBUFFER_HEIGHT not implemented"); return EGL_FALSE;
            case EGL_MAX_PBUFFER_PIXELS: ESP_LOGE(LIBRARY_NAME, "EGL_MAX_PBUFFER_PIXELS not implemented"); return EGL_FALSE;
            case EGL_MAX_SWAP_INTERVAL: ESP_LOGE(LIBRARY_NAME, "EGL_MAX_SWAP_INTERVAL not implemented"); return EGL_FALSE;
            case EGL_MIN_SWAP_INTERVAL: ESP_LOGE(LIBRARY_NAME, "EGL_MIN_SWAP_INTERVAL not implemented"); return EGL_FALSE;
            case EGL_NATIVE_RENDERABLE: ESP_LOGE(LIBRARY_NAME, "EGL_NATIVE_RENDERABLE not implemented"); return EGL_FALSE;
            case EGL_NATIVE_VISUAL_ID: ESP_LOGE(LIBRARY_NAME, "EGL_NATIVE_VISUAL_ID not implemented"); return EGL_FALSE;
            case EGL_NATIVE_VISUAL_TYPE: ESP_LOGE(LIBRARY_NAME, "EGL_NATIVE_VISUAL_TYPE not implemented"); return EGL_FALSE;
            case EGL_RED_SIZE: *value = con->red; return EGL_TRUE;
            case EGL_RENDERABLE_TYPE: *value = con->renderableType; return EGL_TRUE;
            case EGL_SAMPLE_BUFFERS: *value = con->sampleBuffers; return EGL_TRUE;
            case EGL_SAMPLES: *value = con->samples; return EGL_TRUE;
            case EGL_STENCIL_SIZE: *value = con->stencil; return EGL_TRUE;
            case EGL_SURFACE_TYPE: *value = con->surfaceType; return EGL_TRUE;
            case EGL_TRANSPARENT_TYPE: ESP_LOGE(LIBRARY_NAME, "EGL_TRANSPARENT_TYPE not implemented"); return EGL_FALSE;
            case EGL_TRANSPARENT_RED_VALUE: ESP_LOGE(LIBRARY_NAME, "EGL_TRANSPARENT_RED_VALUE not implemented"); return EGL_FALSE;
            case EGL_TRANSPARENT_GREEN_VALUE: ESP_LOGE(LIBRARY_NAME, "EGL_TRANSPARENT_GREEN_VALUE not implemented"); return EGL_FALSE;
            case EGL_TRANSPARENT_BLUE_VALUE: ESP_LOGE(LIBRARY_NAME, "EGL_TRANSPARENT_BLUE_VALUE not implemented"); return EGL_FALSE;
            default: lastError = EGL_BAD_ATTRIBUTE; return EGL_FALSE;
            }
        }
    lastError = EGL_BAD_CONFIG;
    return EGL_FALSE;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (num_config == nullptr)
    {
        lastError = EGL_BAD_PARAMETER;
        return EGL_FALSE;
    }
    ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
    if (configs != nullptr)
    {
        for (int8_t i = 0; i < config_size && i < display->configs.size(); i++)
            configs[i] = &display->configs.at(i);
    }
    *num_config = display->configs.size();
    return EGL_TRUE;
}

EGLContext eglGetCurrentContext()
{
    ESP_LOGE(LIBRARY_NAME, "eglGetCurrentContext unimplemented");
    return nullptr;
}

EGLDisplay eglGetCurrentDisplay()
{
    ESP_LOGE(LIBRARY_NAME, "eglGetCurrentDisplay unimplemented");
    return nullptr;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
    ESP_LOGE(LIBRARY_NAME, "eglGetCurrentSurface unimplemented");
    return nullptr;
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
    switch (display_id)
    {
    case EGL_DEFAULT_DISPLAY:
        return &eglABCv2;
    default:
        ESP_LOGE(LIBRARY_NAME, "eglGetDisplay unimplemented");
        return nullptr;
    }
}

EGLint eglGetError()
{
    EGLint returned = lastError;
    lastError = EGL_SUCCESS;
    return returned;
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform, void* native_display, const EGLAttrib* attrib_list)
{
    ESP_LOGE(LIBRARY_NAME, "eglGetPlatformDisplay unimplemented");
    return nullptr;
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* procname)
{
    return nullptr;
}

EGLBoolean eglGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib* value)
{
    ESP_LOGE(LIBRARY_NAME, "eglGetSyncAttrib unimplemented");
    return false;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (major != nullptr)
        *major = 1;
    if (minor != nullptr)
        *minor = 5;
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (draw == read && read == EGL_NO_SURFACE && ctx == EGL_NO_CONTEXT)
    {
        ABCv2_Display* display = static_cast<ABCv2_Display*>(dpy);
        Context* con = display->context.get();
        Surface* sur = display->surface.get();
        con->surfaceFirstFrameBuffer = &sur->firstFrameBuffer;
        sur->pixels = &con->pixels;
        sur->height = &con->height;
        sur->width = &con->width;
        context = con;
        return EGL_TRUE;
    }
    if (ctx != nullptr && draw != nullptr && read != nullptr)
    {
        Context* con = static_cast<Context*>(ctx);
        Surface* sur = static_cast<Surface*>(read);
        con->surfaceFirstFrameBuffer = &sur->firstFrameBuffer;
        sur->pixels = &con->pixels;
        sur->height = &con->height;
        sur->width = &con->width;
        context = con;
    }
    return EGL_TRUE;
}

EGLenum eglQueryAPI()
{
    ESP_LOGE(LIBRARY_NAME, "eglQueryAPI unimplemented");
    return 0;
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value)
{
    ESP_LOGE(LIBRARY_NAME, "eglQueryContext unimplemented");
    return false;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name)
{
    ESP_LOGE(LIBRARY_NAME, "eglQueryString unimplemented");
    return nullptr;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value)
{
    ESP_LOGE(LIBRARY_NAME, "eglQuerySurface unimplemented");
    return false;
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    ESP_LOGE(LIBRARY_NAME, "eglReleaseTexImage unimplemented");
    return false;
}

EGLBoolean eglReleaseThread()
{
    ESP_LOGE(LIBRARY_NAME, "eglReleaseThread unimplemented");
    return false;
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
    ESP_LOGE(LIBRARY_NAME, "eglSurfaceAttrib unimplemented");
    return false;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (surface == nullptr)
    {
        lastError = EGL_BAD_SURFACE;
        return EGL_FALSE;
    }
    Surface* sur = (Surface*)surface;
    sur->swapBuffers();
    return EGL_TRUE;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    if (dpy != &eglABCv2)
    {
        lastError = EGL_BAD_DISPLAY;
        return EGL_FALSE;
    }
    if (interval)
        ESP_LOGE(LIBRARY_NAME, "Vertical sync not supported");
    return EGL_TRUE;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
    ESP_LOGE(LIBRARY_NAME, "eglTerminate unimplemented");
    return false;
}

EGLBoolean eglWaitClient()
{
    ESP_LOGE(LIBRARY_NAME, "eglWaitClient unimplemented");
    return false;
}

EGLBoolean eglWaitGL()
{
    ESP_LOGE(LIBRARY_NAME, "eglWaitGL unimplemented");
    return false;
}

EGLBoolean eglWaitNative(EGLint engine)
{
    ESP_LOGE(LIBRARY_NAME, "eglWaitNative unimplemented");
    return false;
}

EGLBoolean eglWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags)
{
    ESP_LOGE(LIBRARY_NAME, "eglWaitSync unimplemented");
    return false;
}