# Third-Party Licenses

This document lists third-party libraries, fonts, and assets used in Lymalink and its associated components, along with their respective licenses.

---

## Libraries

### Qt 6 - Frontend
**Version:** 6.10.3  
**Modules:** Core, GUI, QML, Quick, Widgets, SQL, Network, DBus  
**License:** GNU Lesser General Public License v3.0 (LGPL-3.0)  
**Source / Info:** https://www.qt.io/licensing

Qt is used under the LGPL-3.0.

---

### OpenSSL
**Version:** 3.5.4  
**License:** Apache License 2.0  
**Source / Info:** https://www.openssl.org/source/license.html

---

### sdbus-c++
**Version:** 2.1.0  
**License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)  
**Source / Info:** https://github.com/Kistler-Group/sdbus-cpp

---

### libcanberra
**Version:** 0.30  
**License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)  
**Source / Info:** http://0pointer.de/lennart/projects/libcanberra

---

### SQLite
**Version:** 3.50.2  
**License:** Public Domain (blessing)  
**Source / Info:** https://www.sqlite.org/copyright.html

SQLite is released into the public domain. The authors dedicate the code to the public domain and make no claim of copyright.

---

### gdk-pixbuf
**License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)  
**Source / Info:** https://gitlab.gnome.org/GNOME/gdk-pixbuf

gdk-pixbuf is used for image loading and pixel buffer handling.

---

### Dear ImGui
**License:** MIT License  
**Source / Info:** https://github.com/ocornut/imgui

Dear ImGui is used for immediate-mode GUI rendering. Backends used: OpenGL3, Vulkan (`imgui_impl_opengl3`, `imgui_impl_vulkan`).

---

### Vulkan
**License:** Apache License 2.0  
**Source / Info:** https://www.khronos.org/vulkan / https://github.com/KhronosGroup/Vulkan-Headers

Vulkan headers and loader are provided by the Khronos Group. Lymalink uses Vulkan as a rendering backend via the system-provided Vulkan loader (`libvulkan`).

---

### JSON for Modern C++ (nlohmann/json)

**License:** MIT License

**Source / Info:** [https://github.com/nlohmann/json](https://github.com/nlohmann/json)

nlohmann/json is used for parsing, manipulating, and validating JSON configuration and achievement data.

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

## C++ Standard Library & System Interfaces
Lymalink uses the C++20 standard library and standard POSIX/Linux system interfaces (`<signal.h>`, `<sys/socket.h>`, `<sys/inotify.h>`, `<unistd.h>`, `<dlfcn.h>`, etc.). These are provided by the system toolchain (e.g. libstdc++ / libc++) and are covered by their respective system library licenses, typically GCC Runtime Library Exception or equivalent.
EGL and GLX headers (`<EGL/egl.h>`, `<GL/glx.h>`) are provided by the system graphics stack and are covered by their respective system library licenses.