/////////////////////////////////////////////////////////
// File: OpenGLOverlayLayer.cpp
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows OpenGL overlay hooks driven by
//              MinHook and Dear ImGui OpenGL3 backend.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <gl/GL.h>

#include "FontEmbedded.h"
#include "OverlaySharedMemoryState.h"
#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "MinHook.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifndef GL_DRAW_FRAMEBUFFER
    #define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
    #define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif

namespace
{
using PFN_SwapBuffers = BOOL(WINAPI*)(HDC);
using PFN_wglSwapLayerBuffers = BOOL(WINAPI*)(HDC, UINT);
using PFN_wglGetProcAddress = PROC(WINAPI*)(LPCSTR);
using PFN_GetProcAddress = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using PFN_glBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);

static PFN_SwapBuffers s_realSwapBuffers = nullptr;
static PFN_wglSwapLayerBuffers s_realWglSwapLayerBuffers = nullptr;
static PFN_wglGetProcAddress s_realWglGetProcAddress = nullptr;
static PFN_GetProcAddress s_realGetProcAddress = nullptr;
static PFN_glBindFramebuffer s_glBindFramebuffer = nullptr;

// One receiver and one ImGui context are shared by every OpenGL swap hook in this process
static WinOverlayReceiver s_overlay;
static std::once_flag s_imguiInitFlag;
static std::mutex s_renderMutex;
static std::atomic_bool s_hooksReady{false};
static std::atomic_bool s_shuttingDown{false};
static thread_local bool s_rendering = false;   // Prevents recursive swap interception while ImGui renders

static GLuint s_iconTexture = 0;
static uint64_t s_iconGeneration = 0;
static std::atomic_bool s_loggedSwapHit{false};
static std::atomic_bool s_loggedNoContext{false};
static std::atomic_bool s_loggedWglProcSwap{false};
static std::atomic_bool s_loggedWglProcLayerSwap{false};
static std::atomic_bool s_loggedGetProcWglGetProc{false};
static std::atomic_bool s_loggedGetProcSwap{false};
static std::atomic_bool s_loggedGetProcLayerSwap{false};

/////////////////////////////////////////////////////////////////////

std::string HookStatus(const char* name, MH_STATUS status)
{
    return std::string(name) + " status=" + std::to_string(static_cast<int>(status));
}

/////////////////////////////////////////////////////////////////////

void LogOnce(std::atomic_bool& flag, const std::string& message)
{
    if (!flag.exchange(true))
    {
        LYMALINK_LOG(message);
    }
}

/////////////////////////////////////////////////////////////////////

uint32_t QueryFramebufferSize(HDC dc, uint32_t& outWidth, uint32_t& outHeight)
{
    outWidth = 0;
    outHeight = 0;

    // Prefer the target window size; some legacy OpenGL games leave viewport state stale at swap
    HWND window = WindowFromDC(dc);
    RECT rect{};
    if (window && GetClientRect(window, &rect))
    {
        const LONG width = rect.right - rect.left;
        const LONG height = rect.bottom - rect.top;
        if (width > 0 && height > 0)
        {
            outWidth = static_cast<uint32_t>(width);
            outHeight = static_cast<uint32_t>(height);
            return 1;
        }
    }

    GLint viewport[4]{};
    // Fallback for memory DCs or unusual swap targets that do not map back to a HWND
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0)
    {
        outWidth = static_cast<uint32_t>(viewport[2]);
        outHeight = static_cast<uint32_t>(viewport[3]);
        return 1;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////

void InitImGui(uint32_t width, uint32_t height)
{
    std::call_once(s_imguiInitFlag, [width, height]() {
        // Bind Dear ImGui to the game's current OpenGL context on the first real swap
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        OverlayFonts::EnsureEmbeddedFontLoaded();

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplOpenGL3_Init("#version 130"))
        {
            LYMALINK_LOG("[OpenGLOverlayLayer][InitImGui] ImGui_ImplOpenGL3_Init failed.");
        }
        else
        {
            LYMALINK_LOG("[OpenGLOverlayLayer][InitImGui] ready " + std::to_string(width) + "x" + std::to_string(height));
        }
    });
}

/////////////////////////////////////////////////////////////////////

PFN_glBindFramebuffer ResolveBindFramebuffer()
{
    if (!s_glBindFramebuffer && s_realWglGetProcAddress)
    {
        // Resolve lazily from WGL so fixed-function-era games do not need FBO entry points at startup
        s_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(s_realWglGetProcAddress("glBindFramebuffer"));
    }
    return s_glBindFramebuffer;
}

/////////////////////////////////////////////////////////////////////

ImTextureID EnsureOpenGLIconTexture(const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
{
    // Upload notification icons only when the producer updates the shared-memory generation
    if (rgbaPixels.size() != OVERLAY_ICON_DATA_SIZE)
    {
        return ImTextureID_Invalid;
    }
    if (s_iconTexture != 0 && s_iconGeneration == generation)
    {
        return static_cast<ImTextureID>(static_cast<uintptr_t>(s_iconTexture));
    }

    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);

    if (s_iconTexture == 0)
    {
        glGenTextures(1, &s_iconTexture);
    }

    glBindTexture(GL_TEXTURE_2D, s_iconTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OVERLAY_ICON_SIZE, OVERLAY_ICON_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels.data());

    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    s_iconGeneration = generation;
    return static_cast<ImTextureID>(static_cast<uintptr_t>(s_iconTexture));
}

/////////////////////////////////////////////////////////////////////

void RenderOverlay(HDC dc, const char* swapPath)
{
    // Swap hooks can fire from helper threads or teardown paths; only render with an active WGL context
    if (s_rendering || s_shuttingDown.load() || !wglGetCurrentContext())
    {
        if (!s_rendering && !s_shuttingDown.load() && !s_loggedNoContext.exchange(true))
        {
            LYMALINK_LOG("[OpenGLOverlayLayer][RenderOverlay] skipped: no current WGL context.");
        }
        return;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (!QueryFramebufferSize(dc, width, height) || width == 0 || height == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;

    // Draw into the back buffer immediately before the game presents it
    InitImGui(width, height);
    ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));

    GLint previousViewport[4]{};
    GLint previousScissor[4]{};
    GLint previousDrawFramebuffer = 0;
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);

    PFN_glBindFramebuffer bindFramebuffer = ResolveBindFramebuffer();
    if (bindFramebuffer)
    {
        // Some engines may leave an offscreen FBO bound at swap; draw overlay to the visible backbuffer
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
        if (previousDrawFramebuffer != 0)
        {
            bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    const bool claimedNotification = s_overlay.BeginFrame();
    ImTextureID icon = EnsureOpenGLIconTexture(s_overlay.IconPixels(), s_overlay.IconGeneration());
    s_overlay.Draw(width, height, icon);
    if (claimedNotification)
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][RenderOverlay] claimed notification; renderer=opengl path=" +
            std::string(swapPath) + " size=" + std::to_string(width) + "x" + std::to_string(height));
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (bindFramebuffer && previousDrawFramebuffer != 0)
    {
        bindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    }
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glScissor(previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3]);

    s_rendering = false;
}

/////////////////////////////////////////////////////////////////////

BOOL WINAPI Hook_SwapBuffers(HDC dc)
{
    // Main GDI swap path used by most WGL games
    if (!s_loggedSwapHit.exchange(true))
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][Hook_SwapBuffers] first swap intercepted.");
    }
    RenderOverlay(dc, "SwapBuffers");
    return s_realSwapBuffers ? s_realSwapBuffers(dc) : FALSE;
}

/////////////////////////////////////////////////////////////////////

BOOL WINAPI Hook_wglSwapLayerBuffers(HDC dc, UINT planes)
{
    // Alternate WGL swap path used by some older engines and compatibility wrappers
    if (!s_loggedSwapHit.exchange(true))
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][Hook_wglSwapLayerBuffers] first swap intercepted.");
    }
    RenderOverlay(dc, "wglSwapLayerBuffers");
    return s_realWglSwapLayerBuffers ? s_realWglSwapLayerBuffers(dc, planes) : FALSE;
}

/////////////////////////////////////////////////////////////////////

PROC WINAPI Hook_wglGetProcAddress(LPCSTR name)
{
    // Mirror the Linux preloader: if an engine requests swap through the GL proc loader, hand back our hook
    if (name && (std::strcmp(name, "SwapBuffers") == 0 || std::strcmp(name, "wglSwapBuffers") == 0))
    {
        LogOnce(s_loggedWglProcSwap, std::string("[OpenGLOverlayLayer][Hook_wglGetProcAddress] returning SwapBuffers hook for ") + name);
        return reinterpret_cast<PROC>(&Hook_SwapBuffers);
    }
    if (name && std::strcmp(name, "wglSwapLayerBuffers") == 0)
    {
        LogOnce(s_loggedWglProcLayerSwap, "[OpenGLOverlayLayer][Hook_wglGetProcAddress] returning wglSwapLayerBuffers hook.");
        return reinterpret_cast<PROC>(&Hook_wglSwapLayerBuffers);
    }
    return s_realWglGetProcAddress ? s_realWglGetProcAddress(name) : nullptr;
}

/////////////////////////////////////////////////////////////////////

bool IsModule(HMODULE module, const wchar_t* expected)
{
    return module && module == GetModuleHandleW(expected);
}

/////////////////////////////////////////////////////////////////////

FARPROC WINAPI Hook_GetProcAddress(HMODULE module, LPCSTR name)
{
    if (!name || IS_INTRESOURCE(name))
    {
        return s_realGetProcAddress ? s_realGetProcAddress(module, name) : nullptr;
    }

    // Legacy 32-bit OpenGL engines often resolve WGL entry points through kernel32!GetProcAddress
    if (IsModule(module, L"gdi32.dll") && std::strcmp(name, "SwapBuffers") == 0)
    {
        LogOnce(s_loggedGetProcSwap, "[OpenGLOverlayLayer][Hook_GetProcAddress] returning SwapBuffers hook.");
        return reinterpret_cast<FARPROC>(&Hook_SwapBuffers);
    }
    if (IsModule(module, L"opengl32.dll"))
    {
        if (std::strcmp(name, "wglGetProcAddress") == 0)
        {
            LogOnce(s_loggedGetProcWglGetProc, "[OpenGLOverlayLayer][Hook_GetProcAddress] returning wglGetProcAddress hook.");
            return reinterpret_cast<FARPROC>(&Hook_wglGetProcAddress);
        }
        if (std::strcmp(name, "wglSwapLayerBuffers") == 0)
        {
            LogOnce(s_loggedGetProcLayerSwap, "[OpenGLOverlayLayer][Hook_GetProcAddress] returning wglSwapLayerBuffers hook.");
            return reinterpret_cast<FARPROC>(&Hook_wglSwapLayerBuffers);
        }
        if (std::strcmp(name, "SwapBuffers") == 0 || std::strcmp(name, "wglSwapBuffers") == 0)
        {
            LogOnce(s_loggedGetProcSwap, std::string("[OpenGLOverlayLayer][Hook_GetProcAddress] returning SwapBuffers hook for ") + name);
            return reinterpret_cast<FARPROC>(&Hook_SwapBuffers);
        }
    }

    return s_realGetProcAddress ? s_realGetProcAddress(module, name) : nullptr;
}

/////////////////////////////////////////////////////////////////////

void CreateHook(void* target, void* detour, void** original, const char* name)
{
    if (!target)
    {
        LYMALINK_LOG(std::string("[OpenGLOverlayLayer][CreateHook] missing target ") + name);
        return;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][CreateHook] " + HookStatus(name, status));
        return;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][EnableHook] " + HookStatus(name, status));
    }
}

/////////////////////////////////////////////////////////////////////

DWORD WINAPI InitThread(LPVOID)
{
    // DllMain starts this worker to avoid doing loader-sensitive MinHook work under the loader lock
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
    if (!gdi32)
    {
        gdi32 = LoadLibraryW(L"gdi32.dll");
    }
    HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    if (!opengl32)
    {
        opengl32 = LoadLibraryW(L"opengl32.dll");
    }

    // Load OpenGL/GDI exports on demand so injected processes do not need to import OpenGL themselves
    if (!kernel32 || !gdi32 || !opengl32)
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][InitThread] required modules are missing.");
        return 1;
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][InitThread] " + HookStatus("MH_Initialize", status));
        return 1;
    }

    // Patch both direct swap exports and proc-address lookup paths used by legacy engines
    CreateHook(reinterpret_cast<void*>(GetProcAddress(gdi32, "SwapBuffers")), reinterpret_cast<void*>(&Hook_SwapBuffers), reinterpret_cast<void**>(&s_realSwapBuffers), "SwapBuffers");
    CreateHook(reinterpret_cast<void*>(GetProcAddress(opengl32, "wglSwapLayerBuffers")), reinterpret_cast<void*>(&Hook_wglSwapLayerBuffers), reinterpret_cast<void**>(&s_realWglSwapLayerBuffers), "wglSwapLayerBuffers");
    CreateHook(reinterpret_cast<void*>(GetProcAddress(opengl32, "wglGetProcAddress")), reinterpret_cast<void*>(&Hook_wglGetProcAddress), reinterpret_cast<void**>(&s_realWglGetProcAddress), "wglGetProcAddress");
    CreateHook(reinterpret_cast<void*>(GetProcAddress(kernel32, "GetProcAddress")), reinterpret_cast<void*>(&Hook_GetProcAddress), reinterpret_cast<void**>(&s_realGetProcAddress), "GetProcAddress");

    s_hooksReady.store(true);
    LYMALINK_LOG("[OpenGLOverlayLayer][InitThread] hooks installed.");
    return 0;
}
}

/////////////////////////////////////////////////////////////////////
// Called from WinOverlayEntrypoint.cpp DllMain when this target defines
// LYMALINK_OVERLAY_ATTACH_HOOKS.
/////////////////////////////////////////////////////////////////////

extern "C" void LymalinkOverlayOnProcessAttach(HINSTANCE instance)
{
    DisableThreadLibraryCalls(instance);

    HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (thread)
    {
        CloseHandle(thread);
    }
    else
    {
        LYMALINK_LOG("[OpenGLOverlayLayer][Attach] CreateThread failed error=" + std::to_string(GetLastError()));
    }
}

/////////////////////////////////////////////////////////////////////

extern "C" void LymalinkOverlayOnProcessDetach()
{
    s_shuttingDown.store(true);
    if (s_hooksReady.load())
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    if (s_iconTexture != 0 && wglGetCurrentContext())
    {
        glDeleteTextures(1, &s_iconTexture);
        s_iconTexture = 0;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }
    s_overlay.Shutdown();
}
