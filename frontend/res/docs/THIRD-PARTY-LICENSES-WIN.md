# Third-Party Licenses

This document lists third-party libraries, fonts, and assets used in Lymalink and its associated components, along with their respective licenses.

---

## Libraries

### Qt 6 - Frontend
**Modules:** Core, GUI, QML, Quick, Widgets, SQL, Network, Quick Controls 2  
**License:** GNU Lesser General Public License v3.0 (LGPL-3.0)  
**Source / Info:** https://www.qt.io/licensing

Qt is used under the LGPL-3.0.

---

### OpenSSL
**License:** Apache License 2.0  
**Source / Info:** https://www.openssl.org/source/license.html

---

### SQLite
**License:** Public Domain (blessing)  
**Source / Info:** https://www.sqlite.org/copyright.html

SQLite is released into the public domain. The authors dedicate the code to the public domain and make no claim of copyright.

---

### JSON for Modern C++ (nlohmann/json)
**License:** MIT License  
**Source / Info:** [https://github.com/nlohmann/json](https://github.com/nlohmann/json)

nlohmann/json is used for parsing, manipulating, and validating JSON configuration and achievement data.

---

### miniaudio
**License:** Public Domain or MIT No Attribution (MIT-0)  
**Source / Info:** https://github.com/mackron/miniaudio

miniaudio is used for notification sound playback and custom sound decoding on Windows.

---

### stb_vorbis
**License:** Public Domain or MIT License  
**Source / Info:** https://github.com/nothings/stb

stb_vorbis is used by the Windows backend through miniaudio for Ogg Vorbis decoding support.

---

### Dear ImGui
**License:** MIT License  
**Source / Info:** https://github.com/ocornut/imgui

Dear ImGui is used for immediate-mode GUI rendering. Backends used: Vulkan, OpenGL3, DirectX 9, DirectX 10, DirectX 11, DirectX 12 (`imgui_impl_vulkan`, `imgui_impl_opengl3`, `imgui_impl_dx9`, `imgui_impl_dx10`, `imgui_impl_dx11`, `imgui_impl_dx12`).

---

### MinHook
**License:** BSD 2-Clause License  
**Source / Info:** https://github.com/TsudaKageyu/minhook

MinHook is used for OpenGL and Direct3D presentation hooks on Windows.

---

### Vulkan
**License:** Apache License 2.0  
**Source / Info:** https://www.khronos.org/vulkan / https://github.com/KhronosGroup/Vulkan-Headers

Vulkan headers and loader are provided by the Khronos Group. Lymalink uses Vulkan as a rendering backend via the system-provided Vulkan loader.

---

### NSIS
**License:** zlib/libpng License
**Source / Info:** https://nsis.sourceforge.io/License / https://nsis.sourceforge.io/

NSIS is used to build the per-user Windows installer and uninstaller for Lymalink.

---

## Fonts

### Inter
**License:** SIL Open Font License 1.1 (OFL-1.1)  
**Author:** Rasmus Andersson  
**Source:** https://rsms.me/inter

The full license text is available in the source repository at `frontend/res/fonts/Inter/OFL.txt`.

---

## Trademarks & Brand Assets

### GitHub Invertocat Logo
The GitHub Invertocat logo is used solely to identify and link to this project's GitHub repository, in accordance with GitHub's Brand Guidelines.

GITHUB®, the GITHUB® logo design, the INVERTOCAT logo design, OCTOCAT®, and the OCTOCAT® logo design are trademarks of GitHub, Inc., registered in the United States and other countries.

This application and its authors are **not** affiliated with, endorsed by, or otherwise connected with GitHub, Inc. Use of the logo is limited to repository identification and reference as permitted under GitHub's Brand Guidelines: https://brand.github.com/foundations/logo

---

## C++ Standard Library & Windows System Interfaces
Lymalink uses the C++20 standard library and standard Windows system interfaces (`<windows.h>`, Winsock / local IPC facilities, registry APIs, COM, Windows Imaging Component, OpenGL, Direct3D 9/10/11/12, DXGI, GDI, User32, Advapi32, etc.). These are provided by the system toolchain, Windows SDK, or Windows operating system and are covered by their respective Microsoft license terms.
