# Lymalink Overlay

Standalone Vulkan and OpenGL overlay libraries for Lymalink.

## Build

From repository root:

```bash
backend-overlay/build.sh clean
backend-overlay/build.sh debug
backend-overlay/build.sh release
```

Release builds compile overlay logging out fully. Debug builds retain logging.

## Deploy

```bash
backend-overlay/build.sh deploy
backend-overlay/build.sh deploy --debug
backend-overlay/build.sh uninstall
```

Deploy installs:

- libraries: `~/.local/lib/lymalink-overlay*.so`
- Vulkan layer manifest: `${XDG_DATA_HOME:-~/.local/share}/vulkan/implicit_layer.d/lymalink_overlay.json`
- OpenGL launcher: `~/.local/bin/lymalink-overlay`
- Flatpak VulkanLayer extension: user Flatpak runtime, when Flatpak is available

## Direct Makefile Build

```bash
make -C backend-overlay BUILD=debug
make -C backend-overlay BUILD=release
```

## Backend Overlay Layout

```text
backend-overlay
├── build.sh
├── include
│   ├── OverlaySharedMemoryState.h
│   └── OverlaySocketProtocol.h
├── lymalink-overlay.sh
├── Makefile
├── README.md
└── src
    ├── FontEmbedded.h
    ├── GLOverlayOpenGL.cpp
    ├── GLOverlayPreloader.cpp
    ├── Logger.cpp
    ├── Logger.h
    ├── OverlayReceiver.cpp
    ├── OverlayReceiver.h
    ├── VulkanOverlayLayer.cpp
    ├── VulkanOverlayLayer.h
    ├── VulkanOverlayRenderer.cpp
    └── VulkanOverlayRenderer.h
```
