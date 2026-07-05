# Backend Overlay Build

Build information for the Linux Vulkan and OpenGL overlay libraries.

## Linux

### Supported Build Host

- Linux `x86_64`
- Flatpak extension branch `25.08`

### Dependencies

Install these packages, or your distro's equivalent:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install \
  build-essential \
  g++-multilib \
  flatpak \
  libegl-dev:i386 \
  libegl1-mesa-dev \
  libgdk-pixbuf-2.0-dev:i386 \
  libgdk-pixbuf-2.0-dev \
  libgl-dev:i386 \
  libgl1-mesa-dev \
  libvulkan-dev:i386 \
  libvulkan-dev \
  make \
  pkg-config \
  unzip \
  wget
```

For Fedora native i386 builds use i686 runtime libraries instead:

```bash
sudo dnf install \
  gcc \
  gcc-c++ \
  glibc-devel.i686 \
  libstdc++-devel.i686 \
  libatomic.i686 \
  vulkan-headers \
  vulkan-loader.i686 \
  gdk-pixbuf2.i686 \
  libglvnd-glx.i686 \
  libglvnd-egl.i686
```

Release builds compile overlay logging out fully. Debug builds retain logging.
Native i386 builds use the host multilib compiler and i386 development
libraries. Flatpak builds run inside `org.freedesktop.Sdk//25.08` so extension
libraries match the Freedesktop runtime ABI. The i386 SDK extension is required
for 32-bit Flatpak overlay artifacts. After compilation, runtime dependency
checks run inside `org.freedesktop.Platform//25.08`. Install the SDK, i386 SDK
extension, and runtime with:

```bash
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.freedesktop.Sdk//25.08
flatpak install --user flathub org.freedesktop.Sdk.Compat.i386//25.08
flatpak install --user flathub org.freedesktop.Platform//25.08
```

### Build Script

From repository root:

```bash
backend-overlay/build.sh clean
backend-overlay/build.sh debug
backend-overlay/build.sh release
backend-overlay/build.sh flatpak-debug
backend-overlay/build.sh flatpak-release
```

### Deploy

```bash
backend-overlay/build.sh deploy
backend-overlay/build.sh deploy --debug
backend-overlay/build.sh uninstall
```

Deploy installs:

- native x86_64 and launcher libraries: `~/.local/lib/lymalink-overlay*.so`
- native i386 libraries: `~/.local/lib/i386-linux-gnu/lymalink-overlay*.so`
- Vulkan layer manifests: `${XDG_DATA_HOME:-~/.local/share}/vulkan/implicit_layer.d/lymalink_overlay.x86_64.json` and `lymalink_overlay.x86.json`
- OpenGL launcher: `~/.local/bin/lymalink-overlay`
- Flatpak VulkanLayer extension with x86_64 and i386 libraries: user Flatpak runtime, when Flatpak is available

### Direct Makefile Build

```bash
make -C backend-overlay BUILD=debug
make -C backend-overlay BUILD=release
```

## Windows

### Supported Build Host

- Windows 10/11 `x86_64`

### Dependencies

- PowerShell 5.1 or later
- CMake
- Ninja
- Vulkan SDK, including headers and both x64/x86 loader import libraries
- Windows SDK Direct3D 9/10/11/12 and DXGI headers and import libraries
  (`d3d9.h`, `d3d9.lib`, `d3d10.h`, `d3d10.lib`, `d3d11.h`,
  `d3d11.lib`, `d3d12.h`, `d3d12.lib`, `dxgi.h`, `dxgi.lib`);
  the legacy DirectX SDK is not required
- Build Tools for Visual Studio 2022
  - Desktop development with C++
  - MSVC v143 C++ x64/x86 build tools
  - Windows 10 or Windows 11 SDK
  - C++ CMake tools for Windows

Install the LunarG Vulkan SDK, then open a new PowerShell terminal:

```powershell
Invoke-WebRequest -Uri 'https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe' -OutFile "$env:TEMP\vulkan-sdk.exe"
Start-Process -FilePath "$env:TEMP\vulkan-sdk.exe" -Wait
```

Verify that the SDK environment variable and headers are available:

```powershell
$env:VULKAN_SDK
Test-Path (Join-Path $env:VULKAN_SDK 'Include\vulkan\vulkan.h')
```

The second command must return `True`. Dear ImGui and MinHook download
automatically on the first build. If script execution is restricted, run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

### Build Script

Run from repository root:

```powershell
.\backend-overlay\build.ps1 clean
.\backend-overlay\build.ps1 debug
.\backend-overlay\build.ps1 release
```

Both x64 and x86 overlay DLLs and injector executables are built under
`backend-overlay\build\windows\<MODE>\<ARCH>\bin\`:

- `lymalink-overlay-vulkan-<ARCH>.dll`
- `lymalink-overlay-opengl-<ARCH>.dll`
- `lymalink-overlay-dx9-<ARCH>.dll`
- `lymalink-overlay-dx10-<ARCH>.dll`
- `lymalink-overlay-dx11-<ARCH>.dll`
- `lymalink-overlay-dx12-<ARCH>.dll`
- `lymalink-overlay-injector-<ARCH>.exe`

### Deploy

```powershell
.\backend-overlay\build.ps1 deploy
.\backend-overlay\build.ps1 deploy --debug
.\backend-overlay\build.ps1 uninstall
```

`deploy` installs both architecture variants to
`%LOCALAPPDATA%\Programs\Lymalink\overlay` and registers their Vulkan implicit
layer manifests for current user. OpenGL, Direct3D 9, Direct3D 10, Direct3D 11, and Direct3D 12 are injected by
`lymalinkd` through the architecture-matched injector helper. `uninstall`
removes those overlay files and registry entries only.

## Backend Overlay Layout

```text
backend-overlay
├── build.ps1
├── build.sh
├── CMakeLists.txt
├── lymalink-overlay.sh
├── Makefile
├── README.md
├── include
│   ├── OverlaySharedMemoryState.h
│   ├── OverlaySocketProtocol.h
│   └── WinOverlaySharedMemoryState.h
└── src
    ├── linux
    │   ├── DlsymResolver.h
    │   ├── FontEmbedded.h
    │   ├── GLOverlayOpenGL.cpp
    │   ├── GLOverlayPreloader.cpp
    │   ├── I386DsoHandle.cpp
    │   ├── Logger.cpp
    │   ├── Logger.h
    │   ├── OverlayReceiver.cpp
    │   ├── OverlayReceiver.h
    │   ├── VulkanOverlayLayer.cpp
    │   ├── VulkanOverlayLayer.h
    │   ├── VulkanOverlayRenderer.cpp
    │   └── VulkanOverlayRenderer.h
    └── win
        ├── FontEmbedded.h
        ├── WinLogger.cpp
        ├── WinLogger.h
        ├── WinOverlayReceiver.cpp
        ├── WinOverlayReceiver.h
        ├── WinOverlayTypes.h
        ├── WinOverlayUi.cpp
        ├── WinOverlayUi.h
        ├── dx
        │   ├── Direct3D9OverlayLayer.cpp
        │   ├── Direct3D10OverlayLayer.cpp
        │   ├── Direct3D11OverlayLayer.cpp
        │   └── Direct3D12OverlayLayer.cpp
        ├── opengl
        │   ├── OpenGLInjector.cpp
        │   └── OpenGLOverlayLayer.cpp
        └── vulkan
            ├── VulkanOverlayLayer.cpp
            ├── VulkanOverlayLayer.h
            ├── VulkanOverlayRenderer.cpp
            └── VulkanOverlayRenderer.h
```
