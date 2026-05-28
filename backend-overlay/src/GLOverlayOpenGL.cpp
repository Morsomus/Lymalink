/////////////////////////////////////////////////////////
// File: GLOverlayOpenGL.cpp
// Date: 2026-05-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: OpenGL overlay renderer loaded by
//              GLOverlayPreloader. Resolves real GLX/EGL
//              functions from libGL/libEGL, renders the
//              overlay before swap and forwards to the
//              real swap function.
/////////////////////////////////////////////////////////

#include "OverlayReceiver.h"
#include "Logger.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include <EGL/egl.h>
#include <GL/glx.h>
#include <cstring>
#include <dlfcn.h>
#include <mutex>

#define LYMALINK_EXPORT __attribute__((visibility("default")))

// Function pointer types for driver symbols and libc dlsym
using PFN_glXSwapBuffers = void (*)(Display*, GLXDrawable);
using PFN_eglSwapBuffers = EGLBoolean (*)(EGLDisplay, EGLSurface);
using PFN_glXGetProcAddress = __GLXextFuncPtr (*)(const GLubyte*);
using PFN_eglGetProcAddress = __eglMustCastToProperFunctionPointerType (*)(const char*);
using PFN_glBindFramebuffer = void (*)(GLenum, GLuint);
using PFN_dlsym = void* (*)(void*, const char*);

/////////////////////////////////////////////////////////////////////

// Real driver swap functions resolved from libGL/libEGL.
static PFN_glXSwapBuffers s_realGLX = nullptr;
static PFN_eglSwapBuffers s_realEGL = nullptr;
static PFN_glBindFramebuffer s_glBindFramebuffer = nullptr;

// One overlay receiver and one ImGui context are shared by the OpenGL hook path
static OverlayReceiver s_overlay;
static std::once_flag s_overlayInitFlag;
static std::once_flag s_imguiInitFlag;
static std::mutex s_renderMtx;

/////////////////////////////////////////////////////////////////////

static PFN_dlsym RealDlsym()
{
    // Use the versioned libc symbol so calls from this renderer bypass the preloader hook
    static auto* realDlsym = reinterpret_cast<PFN_dlsym>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
    return realDlsym;
}

/////////////////////////////////////////////////////////////////////

static void* OpenLibrary(const char* name)
{
    // Try to get a handle to an already-loaded library before forcing a fresh load
    void* handle = dlopen(name, RTLD_NOW | RTLD_NOLOAD);

    // Fall back to a local load if the library is not yet mapped into the process
    if (!handle)
    {
        handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    }
    if (!handle)
    {
        const char* error = dlerror();
        Logger::Log("[GLOverlayOpenGL] failed to open " + std::string(name) + ": " + (error ? error : "unknown dlerror"));
    }

    return handle;
}

/////////////////////////////////////////////////////////////////////

static PFN_glBindFramebuffer ResolveGLBindFramebuffer()
{
    if (s_glBindFramebuffer)
    {
        return s_glBindFramebuffer;
    }

    // Resolve lazily from the active GL loader so GLX and EGL games both work.
    s_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glBindFramebuffer"))); 
    if (!s_glBindFramebuffer)
    {
        s_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(eglGetProcAddress("glBindFramebuffer"));
    }
    
    return s_glBindFramebuffer;
}

/////////////////////////////////////////////////////////////////////

__attribute__((constructor))
static void ResolveRealFunctions()
{
    // Runs automatically when the preloader dlopens this renderer
    void* libGL = OpenLibrary("libGL.so.1");
    void* libEGL = OpenLibrary("libEGL.so.1");
    // Use the unhooked dlsym to avoid infinite recursion through the preloader
    auto* realDlsym = RealDlsym();

    if (!realDlsym)
    {
        Logger::Log("[GLOverlayOpenGL] failed to resolve real dlsym");
        return;
    }

    if (libGL)
    {
        // Grab the real swap function so we can forward after rendering the overlay
        s_realGLX = reinterpret_cast<PFN_glXSwapBuffers>(realDlsym(libGL, "glXSwapBuffers"));
        if (!s_realGLX)
        {
            Logger::Log("[GLOverlayOpenGL] missing symbol glXSwapBuffers from libGL.so.1");
        }
    }
    if (libEGL)
    {
        // Same forwarding pointer for the EGL path
        s_realEGL = reinterpret_cast<PFN_eglSwapBuffers>(realDlsym(libEGL, "eglSwapBuffers"));
        if (!s_realEGL)
        {
            Logger::Log("[GLOverlayOpenGL] missing symbol eglSwapBuffers from libEGL.so.1");
        }
    }

    Logger::Log("[GLOverlayOpenGL] glXSwapBuffers real=" + std::to_string(reinterpret_cast<uintptr_t>(s_realGLX)));
    Logger::Log("[GLOverlayOpenGL] eglSwapBuffers real=" + std::to_string(reinterpret_cast<uintptr_t>(s_realEGL)));

    // Close handles
    if (libGL)
    {
        dlclose(libGL);
    }
    if (libEGL)
    {
        dlclose(libEGL);
    }
}

/////////////////////////////////////////////////////////////////////

static void QueryGLXSize(Display* dpy, GLXDrawable drawable, uint32_t& outW, uint32_t& outH)
{
    // GLX drawables expose dimensions directly through glXQueryDrawable
    unsigned int w = 0;
    unsigned int h = 0;
    glXQueryDrawable(dpy, drawable, GLX_WIDTH, &w);
    glXQueryDrawable(dpy, drawable, GLX_HEIGHT, &h);
    if (w == 0 || h == 0)
    {
        static bool s_loggedZeroSize = false;
        if (!s_loggedZeroSize)
        {
            Logger::Log("[GLOverlayOpenGL] glXQueryDrawable returned zero size " + std::to_string(w) + "x" + std::to_string(h));
            s_loggedZeroSize = true;
        }
    }
    outW = static_cast<uint32_t>(w);
    outH = static_cast<uint32_t>(h);
}

/////////////////////////////////////////////////////////////////////

static void QueryEGLSize(EGLDisplay dpy, EGLSurface surface, uint32_t& outW, uint32_t& outH)
{
    // EGL surfaces expose dimensions through eglQuerySurface.
    EGLint w = 0;
    EGLint h = 0;

    // EGL_WIDTH and EGL_HEIGHT give the current render surface dimensions
    const EGLBoolean widthOk = eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    const EGLBoolean heightOk = eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    if (!widthOk || !heightOk || w <= 0 || h <= 0)
    {
        static bool s_loggedInvalidSize = false;
        if (!s_loggedInvalidSize)
        {
            Logger::Log("[GLOverlayOpenGL] eglQuerySurface failed or returned invalid size " + std::to_string(w) + "x" + std::to_string(h));
            s_loggedInvalidSize = true;
        }
    }
    outW = static_cast<uint32_t>(w);
    outH = static_cast<uint32_t>(h);
}

/////////////////////////////////////////////////////////////////////

static void InitOverlay()
{
    // Initialise exactly once, connection failure is non-fatal and retried per frame
    std::call_once(s_overlayInitFlag, []()
    {
        const bool ready = s_overlay.InitConnection();
        Logger::Log(std::string("[GLOverlayOpenGL] overlay connection ") + (ready ? "initialised" : "not ready"));
    });
}

/////////////////////////////////////////////////////////////////////

static void InitImGui(uint32_t w, uint32_t h)
{
    // Bind ImGui to the game's active GL context, must run only once per context
    std::call_once(s_imguiInitFlag, [w, h]()
    {
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // Set initial display size, updated every frame in RenderOverlay
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));

        // Disable the imgui.ini file so the overlay has no disk footprint
        io.IniFilename = nullptr;

        ImGui::StyleColorsDark();
        if (!ImGui_ImplOpenGL3_Init("#version 130"))
        {
            Logger::Log("[GLOverlayOpenGL] ImGui_ImplOpenGL3_Init failed");
        }
        s_overlay.EnsureOpenGLImGuiContext();

        Logger::Log("[GLOverlayOpenGL] ImGui OpenGL3 ready " + std::to_string(w) + "x" + std::to_string(h));
    });
}

/////////////////////////////////////////////////////////////////////

static void RenderOverlay(uint32_t w, uint32_t h)
{
    // Render into the current back buffer before the real swap function presents it
    std::lock_guard<std::mutex> lock(s_renderMtx);

    // Keep display size in sync with the current drawable dimensions
    ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Let the receiver build and submit the notification for this frame
    s_overlay.RenderNotificationFrame(w, h);

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData)
    {
        // Null draw data means ImGui produced nothing, skip the GL draw call
        static bool s_loggedMissingDrawData = false;
        if (!s_loggedMissingDrawData)
        {
            Logger::Log("[GLOverlayOpenGL] ImGui::GetDrawData returned null");
            s_loggedMissingDrawData = true;
        }
        return;
    }

    // Some engines leave an offscreen FBO bound at swap, draw to the visible backbuffer
    GLint previousDrawFramebuffer = 0;
    PFN_glBindFramebuffer glBindFramebufferFn = ResolveGLBindFramebuffer();
    if (glBindFramebufferFn)
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
        if (previousDrawFramebuffer != 0)
        {
            static bool s_loggedFramebufferBind = false;
            if (!s_loggedFramebufferBind)
            {
                Logger::Log("[GLOverlayOpenGL] binding default draw framebuffer, previous=" + std::to_string(previousDrawFramebuffer));
                s_loggedFramebufferBind = true;
            }
            glBindFramebufferFn(GL_DRAW_FRAMEBUFFER, 0);
        }
    }

    ImGui_ImplOpenGL3_RenderDrawData(drawData);

    if (glBindFramebufferFn && previousDrawFramebuffer != 0)
    {
        // Restore game GL state after our draw so the next frame continues normally
        glBindFramebufferFn(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    }
}

/////////////////////////////////////////////////////////////////////

extern "C"
{

LYMALINK_EXPORT void lymalink_glXSwapBuffers(Display* dpy, GLXDrawable drawable)
{
    // Entry point intercepted from the preloader for X11/GLX games
    uint32_t w = 0;
    uint32_t h = 0;
    QueryGLXSize(dpy, drawable, w, h);

    InitOverlay();
    InitImGui(w, h);

    // Draw the overlay into the back buffer before handing off to the real swap
    RenderOverlay(w, h);

    if (s_realGLX)
    {
        s_realGLX(dpy, drawable);
    }
    else
    {
        static bool s_loggedMissingReal = false;
        if (!s_loggedMissingReal)
        {
            Logger::Log("[GLOverlayOpenGL] real glXSwapBuffers is missing");
            s_loggedMissingReal = true;
        }
    }
}

LYMALINK_EXPORT EGLBoolean lymalink_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    // Entry point intercepted from the preloader for EGL-backed games
    uint32_t w = 0;
    uint32_t h = 0;
    QueryEGLSize(dpy, surface, w, h);

    InitOverlay();
    InitImGui(w, h);

    // Draw the overlay into the back buffer before handing off to the real swap
    RenderOverlay(w, h);

    if (s_realEGL)
    {
        // Forward the call and propagate the return value to the caller
        return s_realEGL(dpy, surface);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        Logger::Log("[GLOverlayOpenGL] real eglSwapBuffers is missing");
        s_loggedMissingReal = true;
    }
    return EGL_FALSE;
}

LYMALINK_EXPORT __GLXextFuncPtr lymalink_glXGetProcAddress(const GLubyte* name)
{
    // If a loader asks for glXSwapBuffers, give it our render-before-swap entry
    if (name && std::strcmp(reinterpret_cast<const char*>(name), "glXSwapBuffers") == 0)
    {
        return reinterpret_cast<__GLXextFuncPtr>(&lymalink_glXSwapBuffers);
    }

    // For all other symbols delegate to the real driver lookup
    auto* realDlsym = RealDlsym();
    auto* real = realDlsym ? reinterpret_cast<PFN_glXGetProcAddress>(realDlsym(RTLD_NEXT, "glXGetProcAddress")) : nullptr;
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        Logger::Log("[GLOverlayOpenGL] real glXGetProcAddress is missing");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

LYMALINK_EXPORT __GLXextFuncPtr lymalink_glXGetProcAddressARB(const GLubyte* name)
{
    // Mirror glXGetProcAddress for loaders using the ARB query function
    if (name && std::strcmp(reinterpret_cast<const char*>(name), "glXSwapBuffers") == 0)
    {
        return reinterpret_cast<__GLXextFuncPtr>(&lymalink_glXSwapBuffers);
    }

    auto* realDlsym = RealDlsym();
    auto* real = realDlsym ? reinterpret_cast<PFN_glXGetProcAddress>(realDlsym(RTLD_NEXT, "glXGetProcAddressARB")) : nullptr;
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        Logger::Log("[GLOverlayOpenGL] real glXGetProcAddressARB is missing");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

LYMALINK_EXPORT __eglMustCastToProperFunctionPointerType lymalink_eglGetProcAddress(const char* name)
{
    // If a loader asks for eglSwapBuffers, give it our render-before-swap entry
    if (name && std::strcmp(name, "eglSwapBuffers") == 0)
    {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(&lymalink_eglSwapBuffers);
    }

    auto* realDlsym = RealDlsym();
    auto* real = realDlsym ? reinterpret_cast<PFN_eglGetProcAddress>(realDlsym(RTLD_NEXT, "eglGetProcAddress")) : nullptr;
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        Logger::Log("[GLOverlayOpenGL] real eglGetProcAddress is missing");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

} // extern "C"
