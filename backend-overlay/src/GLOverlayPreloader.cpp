/////////////////////////////////////////////////////////
// File: GLOverlayPreloader.cpp
// Date: 2026-05-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tiny LD_PRELOAD preloader for OpenGL games.
//              Intercepts GLX/EGL swap and proc lookup
//              calls, loads lymalink-overlay-opengl.so
//              and forwards into it.
/////////////////////////////////////////////////////////

#include "Logger.h"

#include <EGL/egl.h>
#include <GL/glx.h>
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <link.h>
#include <string>

#ifndef RTLD_DEEPBIND
#define RTLD_DEEPBIND 0
#endif

#define LYMALINK_EXPORT __attribute__((visibility("default")))

// Function pointer types for symbols intercepted by this preloader
using PFN_glXSwapBuffers = void (*)(Display*, GLXDrawable);
using PFN_eglSwapBuffers = EGLBoolean (*)(EGLDisplay, EGLSurface);
using PFN_glXGetProcAddress = __GLXextFuncPtr (*)(const GLubyte*);
using PFN_eglGetProcAddress = __eglMustCastToProperFunctionPointerType (*)(const char*);
using PFN_dlsym = void* (*)(void*, const char*);

// Exported symbols must stay visible even though the library is built with hidden visibility
extern "C"
{
    LYMALINK_EXPORT void glXSwapBuffers(Display* dpy, GLXDrawable drawable);
    LYMALINK_EXPORT EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
    LYMALINK_EXPORT __GLXextFuncPtr glXGetProcAddress(const GLubyte* name);
    LYMALINK_EXPORT __GLXextFuncPtr glXGetProcAddressARB(const GLubyte* name);
    LYMALINK_EXPORT __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name);
    LYMALINK_EXPORT void* dlsym(void* handle, const char* name);
}

/////////////////////////////////////////////////////////////////////

// OpenGL renderer library and its private lymalink_* entry points
static void* s_openglLib = nullptr;
static PFN_glXSwapBuffers s_realGLXSwapBuffers = nullptr;
static PFN_eglSwapBuffers s_realEGLSwapBuffers = nullptr;
static PFN_glXGetProcAddress s_realGLXGetProcAddress = nullptr;
static PFN_glXGetProcAddress s_realGLXGetProcAddressARB = nullptr;
static PFN_eglGetProcAddress s_realEGLGetProcAddress = nullptr;

/////////////////////////////////////////////////////////////////////

static void LogMissingSymbol(const char* symbol)
{
    LYMALINK_LOG(std::string("[GLOverlayPreloader] missing symbol: ") + symbol);
}

/////////////////////////////////////////////////////////////////////

static PFN_dlsym RealDlsym()
{
    // Use the versioned libc symbol so our own dlsym hook cannot recurse
    static auto* realDlsym = reinterpret_cast<PFN_dlsym>(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
    return realDlsym;
}

/////////////////////////////////////////////////////////////////////

// Finds the directory containing this preloader so the OpenGL renderer can live beside it
static bool ResolvePreloaderDirectory(char* outPath, size_t outPathSize)
{
    struct SearchContext
    {
        void* self = nullptr;
        char path[4096] = {};
    };

    SearchContext ctx{};
    ctx.self = reinterpret_cast<void*>(&ResolvePreloaderDirectory);

    // Iterate through all loaded shared objects in the process memory
    dl_iterate_phdr([](struct dl_phdr_info* info, size_t, void* data) -> int
    {
        auto* ctx = static_cast<SearchContext*>(data);
        if (!info->dlpi_name || info->dlpi_name[0] == '\0')
        {
            return 0;
        }

        // Check if the current function's memory address falls within this object's segments
        for (int i = 0; i < info->dlpi_phnum; ++i)
        {
            const uintptr_t start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
            const uintptr_t end = start + info->dlpi_phdr[i].p_memsz;
            const uintptr_t self = reinterpret_cast<uintptr_t>(ctx->self);

            if (self >= start && self < end)
            {
                std::snprintf(ctx->path, sizeof(ctx->path), "%s", info->dlpi_name);
                return 1;   // Match found
            }
        }

        return 0;
    }, &ctx);

    if (ctx.path[0] == '\0')
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve preloader path from loaded objects");
        return false;
    }

    // Extract the directory path
    const char* slash = std::strrchr(ctx.path, '/');
    if (!slash)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve preloader directory from loaded object path");
        return false;
    }

    const size_t dirLen = static_cast<size_t>(slash - ctx.path + 1);
    std::snprintf(outPath, outPathSize, "%.*s", static_cast<int>(dirLen), ctx.path);
    return true;
}

/////////////////////////////////////////////////////////////////////

static void LoadOpenGLLib()
{
    // Lazy load keeps startup cheap for helper processes that never touch OpenGL
    if (s_openglLib)
    {
        return;
    }

    char preloaderDir[4096] = {};
    std::string openglPath;

    if (ResolvePreloaderDirectory(preloaderDir, sizeof(preloaderDir)))
    {
        openglPath = std::string(preloaderDir) + "lymalink-overlay-opengl.so";
    }
    else
    {
        openglPath = "lymalink-overlay-opengl.so";
    }

    // Load the real renderer using DEEPBIND to isolate its symbols from game-specific overrides
    s_openglLib = dlopen(openglPath.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if (!s_openglLib)
    {
        const char* error = dlerror();
        LYMALINK_LOG("[GLOverlayPreloader] failed to load " + openglPath + ": " + (error ? error : "unknown dlerror"));
        return;
    }

    auto* realDlsym = RealDlsym();
    if (!realDlsym)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve real dlsym");
        return;
    }

    // Bind the internal lymalink_* forwarder function pointers from the renderer library
    s_realGLXSwapBuffers = reinterpret_cast<PFN_glXSwapBuffers>(realDlsym(s_openglLib, "lymalink_glXSwapBuffers"));
    s_realEGLSwapBuffers = reinterpret_cast<PFN_eglSwapBuffers>(realDlsym(s_openglLib, "lymalink_eglSwapBuffers"));
    s_realGLXGetProcAddress = reinterpret_cast<PFN_glXGetProcAddress>(realDlsym(s_openglLib, "lymalink_glXGetProcAddress"));
    s_realGLXGetProcAddressARB = reinterpret_cast<PFN_glXGetProcAddress>(realDlsym(s_openglLib, "lymalink_glXGetProcAddressARB"));
    s_realEGLGetProcAddress = reinterpret_cast<PFN_eglGetProcAddress>(realDlsym(s_openglLib, "lymalink_eglGetProcAddress"));

    if (!s_realGLXSwapBuffers) LogMissingSymbol("lymalink_glXSwapBuffers");
    if (!s_realEGLSwapBuffers) LogMissingSymbol("lymalink_eglSwapBuffers");
    if (!s_realGLXGetProcAddress) LogMissingSymbol("lymalink_glXGetProcAddress");
    if (!s_realGLXGetProcAddressARB) LogMissingSymbol("lymalink_glXGetProcAddressARB");
    if (!s_realEGLGetProcAddress) LogMissingSymbol("lymalink_eglGetProcAddress");
}

/////////////////////////////////////////////////////////////////////

static void* ResolveRealSymbol(const char* name)
{
    // Fallback path used only if the renderer failed to load or did not expose a forwarder
    auto* realDlsym = RealDlsym();
    if (!realDlsym)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve real dlsym while resolving fallback symbol");
        return nullptr;
    }

    // Pass through to the next library in the dynamic link order (usually the real graphics driver)
    return realDlsym(RTLD_NEXT, name);
}

/////////////////////////////////////////////////////////////////////

extern "C" LYMALINK_EXPORT void* dlsym(void* handle, const char* name)
{
    // Some engines fetch swap functions with dlsym instead of glX/eglGetProcAddress
    if (std::strcmp(name, "glXSwapBuffers") == 0)       return reinterpret_cast<void*>(&glXSwapBuffers);
    if (std::strcmp(name, "eglSwapBuffers") == 0)       return reinterpret_cast<void*>(&eglSwapBuffers);
    if (std::strcmp(name, "glXGetProcAddress") == 0)    return reinterpret_cast<void*>(&glXGetProcAddress);
    if (std::strcmp(name, "glXGetProcAddressARB") == 0) return reinterpret_cast<void*>(&glXGetProcAddressARB);
    if (std::strcmp(name, "eglGetProcAddress") == 0)    return reinterpret_cast<void*>(&eglGetProcAddress);

    auto* realDlsym = RealDlsym();
    if (!realDlsym)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve real dlsym inside dlsym hook");
        return nullptr;
    }

    // Forward all unrelated symbol requests to the standard system dlsym handler
    return realDlsym(handle, name);
}

extern "C"
{

LYMALINK_EXPORT void glXSwapBuffers(Display* dpy, GLXDrawable drawable)
{
    // First GLX swap loads the renderer, renders overlay there, then forwards real swap
    LoadOpenGLLib();
    if (s_realGLXSwapBuffers)
    {
        s_realGLXSwapBuffers(dpy, drawable);
        return;
    }

    // Pass through to the original driver function if the custom renderer is unavailable
    auto* real = reinterpret_cast<PFN_glXSwapBuffers>(ResolveRealSymbol("glXSwapBuffers"));
    if (real)
    {
        real(dpy, drawable);
    }
    else
    {
        static bool s_loggedMissingReal = false;
        if (!s_loggedMissingReal)
        {
            LYMALINK_LOG("[GLOverlayPreloader] failed to resolve fallback real glXSwapBuffers");
            s_loggedMissingReal = true;
        }
    }
}

LYMALINK_EXPORT EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    // EGL path is used by some OpenGL games on X11 and Wayland
    LoadOpenGLLib();
    if (s_realEGLSwapBuffers)
    {
        return s_realEGLSwapBuffers(dpy, surface);
    }

    // Pass through to the original driver function if the custom renderer is unavailable
    auto* real = reinterpret_cast<PFN_eglSwapBuffers>(ResolveRealSymbol("eglSwapBuffers"));
    if (real)
    {
        return real(dpy, surface);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve fallback real eglSwapBuffers");
        s_loggedMissingReal = true;
    }
    return EGL_FALSE;
}

LYMALINK_EXPORT __GLXextFuncPtr glXGetProcAddress(const GLubyte* name)
{
    // Return renderer-owned hooks for GLX names, otherwise pass through to the driver
    LoadOpenGLLib();
    if (s_realGLXGetProcAddress)
    {
        return s_realGLXGetProcAddress(name);
    }

    // Fall back to querying the official graphics library extension loader
    auto* real = reinterpret_cast<PFN_glXGetProcAddress>(ResolveRealSymbol("glXGetProcAddress"));
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve fallback real glXGetProcAddress");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

LYMALINK_EXPORT __GLXextFuncPtr glXGetProcAddressARB(const GLubyte* name)
{
    // Many OpenGL loaders prefer the ARB variant, so it mirrors glXGetProcAddress
    LoadOpenGLLib();
    if (s_realGLXGetProcAddressARB)
    {
        return s_realGLXGetProcAddressARB(name);
    }

    auto* real = reinterpret_cast<PFN_glXGetProcAddress>(ResolveRealSymbol("glXGetProcAddressARB"));
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve fallback real glXGetProcAddressARB");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

LYMALINK_EXPORT __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name)
{
    // Return renderer-owned hooks for EGL names, otherwise pass through to the driver
    LoadOpenGLLib();
    if (s_realEGLGetProcAddress)
    {
        return s_realEGLGetProcAddress(name);
    }

    // Fall back to querying the official EGL extension loader
    auto* real = reinterpret_cast<PFN_eglGetProcAddress>(ResolveRealSymbol("eglGetProcAddress"));
    if (real)
    {
        return real(name);
    }

    static bool s_loggedMissingReal = false;
    if (!s_loggedMissingReal)
    {
        LYMALINK_LOG("[GLOverlayPreloader] failed to resolve fallback real eglGetProcAddress");
        s_loggedMissingReal = true;
    }
    return nullptr;
}

} // extern "C"
