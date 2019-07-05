// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/GrContext.h"
#include "include/gpu/gl/GrGLInterface.h"

#define LTRACEF(fmt, ...) printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ## __VA_ARGS__)

#define FB_WIDTH  1280
#define FB_HEIGHT 720

int nx_link_sock = -1;

void draw(int x, int y, SkCanvas& canvas) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLUE);
    canvas.drawCircle(x + 128, y + 128, 90, paint);
    paint.setColor(SK_ColorWHITE);
    canvas.drawCircle(x + 86, y + 86, 20, paint);
    canvas.drawCircle(x + 160, y + 76, 20, paint);
    canvas.drawCircle(x + 140, y + 150, 35, paint);
}

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static bool initEgl(NWindow* win)
{
    // Connect to the EGL default display
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!s_display)
    {
        LTRACEF("Could not connect to display! error: %d", eglGetError());
        goto _fail0;
    }

    // Initialize the EGL display connection
    eglInitialize(s_display, nullptr, nullptr);

    // Select OpenGL (Core) as the desired graphics API
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        LTRACEF("Could not set API! error: %d", eglGetError());
        goto _fail1;
    }

    // Get an appropriate EGL framebuffer configuration
    EGLConfig config;
    EGLint numConfigs;
    static const EGLint framebufferAttributeList[] =
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   8,
        EGL_DEPTH_SIZE,   24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);
    if (numConfigs == 0)
    {
        LTRACEF("No config found! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL window surface
    s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);
    if (!s_surface)
    {
        LTRACEF("Surface creation failed! error: %d", eglGetError());
        goto _fail1;
    }

    // Create an EGL rendering context
    static const EGLint contextAttributeList[] =
    {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 3,
        EGL_NONE
    };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
    if (!s_context)
    {
        LTRACEF("Context creation failed! error: %d", eglGetError());
        goto _fail2;
    }

    // Connect the context to the surface
    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    return true;

_fail2:
    eglDestroySurface(s_display, s_surface);
    s_surface = nullptr;
_fail1:
    eglTerminate(s_display);
    s_display = nullptr;
_fail0:
    return false;
}

static void deinitEgl()
{
    if (s_display)
    {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context)
        {
            eglDestroyContext(s_display, s_context);
            s_context = nullptr;
        }
        if (s_surface)
        {
            eglDestroySurface(s_display, s_surface);
            s_surface = nullptr;
        }
        eglTerminate(s_display);
        s_display = nullptr;
    }
}

extern "C" void userAppInit(void)
{
    socketInitializeDefault();
    nx_link_sock = nxlinkStdio();
}

extern "C" void userAppExit(void)
{
    close(nx_link_sock);
    socketExit();
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    // Other initialization goes here. As a demonstration, we print hello world.
    printf("Hello World!!\n");

    if (!initEgl(nwindowGetDefault()))
        return EXIT_FAILURE;

    LTRACEF("GrGLMakeNativeInterface");
    auto interface = GrGLMakeNativeInterface();

    LTRACEF("GrContext::MakeGL");
    auto ctx = GrContext::MakeGL();

    GrGLint buffer;
    interface->fFunctions.fGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
    GrGLFramebufferInfo info;
    info.fFormat = GL_RGBA8;
    info.fFBOID = (GrGLuint)buffer;

    GrBackendRenderTarget target(FB_WIDTH, FB_HEIGHT, 0, 8, info);

    SkSurfaceProps props(SkSurfaceProps::kLegacyFontHost_InitType);

    auto surface = SkSurface::MakeFromBackendRenderTarget(ctx.get(), target,
            kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
            nullptr, &props);

    SkCanvas* canvas = surface->getCanvas();

    int x = 0;

    // Main loop
    while (appletMainLoop())
    {
        // Scan all the inputs. This should be done once for each frame
        hidScanInput();

        // hidKeysDown returns information about which buttons have been
        // just pressed in this frame compared to the previous one
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS)
            break; // break in order to return to hbmenu

        canvas->clear(SK_ColorBLACK);

        x += 10;
        if(x > FB_WIDTH)
        {
            x = 0;
        }
        draw(x, 10, *canvas);
        draw(x + 200, 10, *canvas);

        draw(x, 210, *canvas);

        canvas->flush();
        eglSwapBuffers(s_display, s_surface);
    }

    deinitEgl();

    return 0;
}
