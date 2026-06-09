#########################################################
# File: Makefile - Overlay Library
# Date: 2026-05-28
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Builds three overlay libraries:
#   lymalink-overlay.so        	  - Vulkan implicit layer
#   lymalink-overlay-opengl.so    - OpenGL renderer
#   lymalink-overlay-preloader.so - LD_PRELOAD preloader
#########################################################

CXX ?= g++
AR ?= ar
BUILD ?= debug
BUILD_ROOT ?= build
ARCH ?= x86_64
BUILD_PROFILE := $(firstword $(subst -, ,$(BUILD)))

BUILD_DIR := $(BUILD_ROOT)/$(BUILD)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

IMGUI_DIR ?= src/imgui
IMGUI_CFLAGS := -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends

CPPFLAGS := -Iinclude -Isrc -I../backend/src
CPPFLAGS_release := -DLYMALINK_OVERLAY_DISABLE_LOGGING
CPPFLAGS += $(CPPFLAGS_$(BUILD_PROFILE))
ARCH_CXXFLAGS :=
ARCH_LDFLAGS :=
ARCH_EXTRA_OBJECTS :=
ARCH_STUB_LIB :=
PKG_CONFIG_ENV :=
GL_LIBS := -lGL
EGL_LIBS := -lEGL

ifeq ($(ARCH),i386)
ARCH_CXXFLAGS := -m32
ARCH_LDFLAGS := -nostartfiles -L$(OBJ_DIR)/i386-libgcc-stub -L/usr/lib/i386-linux-gnu
ARCH_EXTRA_OBJECTS := $(OBJ_DIR)/$(ARCH)/src/I386DsoHandle.o
ARCH_STUB_LIB := $(OBJ_DIR)/i386-libgcc-stub/libgcc.a
VULKAN_LIBS_OVERRIDE := -l:libvulkan.so.1
GDK_PIXBUF_LIBS_OVERRIDE := -l:libgdk_pixbuf-2.0.so.0 -l:libgobject-2.0.so.0 -l:libglib-2.0.so.0
GL_LIBS := -l:libGL.so.1
EGL_LIBS := -l:libEGL.so.1
endif

CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -pedantic $(ARCH_CXXFLAGS)
CXXFLAGS_debug := -O0 -g
CXXFLAGS_release := -O2 -DNDEBUG
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_$(BUILD_PROFILE))
DEPFLAGS := -MMD -MP

SHARED_CXXFLAGS := $(CXXFLAGS) -fPIC -fvisibility=hidden
IMGUI_BUILD_CXXFLAGS := $(SHARED_CXXFLAGS) $(IMGUI_CFLAGS)
OPENGL_CFLAGS := -DLYMALINK_OVERLAY_OPENGL_TEXTURES

VULKAN_CFLAGS := $(shell $(PKG_CONFIG_ENV) pkg-config --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(or $(VULKAN_LIBS_OVERRIDE),$(shell $(PKG_CONFIG_ENV) pkg-config --libs vulkan 2>/dev/null || echo -lvulkan))
GDK_PIXBUF_CFLAGS := $(shell $(PKG_CONFIG_ENV) pkg-config --cflags gdk-pixbuf-2.0 2>/dev/null)
GDK_PIXBUF_LIBS := $(or $(GDK_PIXBUF_LIBS_OVERRIDE),$(shell $(PKG_CONFIG_ENV) pkg-config --libs gdk-pixbuf-2.0 2>/dev/null || echo -lgdk_pixbuf-2.0))

# Source groups
COMMON_SOURCES := \
    src/OverlayReceiver.cpp \
    src/Logger.cpp \
    ../backend/src/tools/Utils.cpp

IMGUI_SOURCES := \
    $(IMGUI_DIR)/imgui.cpp \
    $(IMGUI_DIR)/imgui_draw.cpp \
    $(IMGUI_DIR)/imgui_tables.cpp \
    $(IMGUI_DIR)/imgui_widgets.cpp

VULKAN_SOURCES := \
    src/VulkanOverlayRenderer.cpp \
    src/VulkanOverlayLayer.cpp \
    $(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp

OPENGL_SOURCES := \
    $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# Per-target sources
VULKAN_TARGET_SOURCES := $(COMMON_SOURCES) $(IMGUI_SOURCES) $(VULKAN_SOURCES)
OPENGL_TARGET_SOURCES := $(COMMON_SOURCES) $(IMGUI_SOURCES) $(OPENGL_SOURCES) src/GLOverlayOpenGL.cpp
PRELOADER_TARGET_SOURCES := \
    src/GLOverlayPreloader.cpp \
    src/Logger.cpp

VULKAN_OBJECTS := $(VULKAN_TARGET_SOURCES:%.cpp=$(OBJ_DIR)/vulkan/%.o)
OPENGL_OBJECTS := $(OPENGL_TARGET_SOURCES:%.cpp=$(OBJ_DIR)/opengl/%.o)
PRELOADER_OBJECTS := $(PRELOADER_TARGET_SOURCES:%.cpp=$(OBJ_DIR)/preloader/%.o)
ifeq ($(ARCH),i386)
VULKAN_OBJECTS += $(ARCH_EXTRA_OBJECTS)
OPENGL_OBJECTS += $(ARCH_EXTRA_OBJECTS)
PRELOADER_OBJECTS += $(ARCH_EXTRA_OBJECTS)
endif
ALL_OBJECTS := $(VULKAN_OBJECTS) $(OPENGL_OBJECTS) $(PRELOADER_OBJECTS)
ALL_DEPS := $(ALL_OBJECTS:.o=.d)

.PHONY: all clean overlay

all: \
    $(BIN_DIR)/lymalink-overlay.so \
    $(BIN_DIR)/lymalink-overlay-opengl.so \
    $(BIN_DIR)/lymalink-overlay-preloader.so

# Vulkan layer
$(BIN_DIR)/lymalink-overlay.so: $(VULKAN_OBJECTS) $(ARCH_STUB_LIB) | $(BIN_DIR)
		@echo "==> Linking lymalink-overlay.so..."
	$(CXX) $(SHARED_CXXFLAGS) $(ARCH_LDFLAGS) -shared -o $@ $^ \
		-Wl,--no-undefined $(VULKAN_LIBS) $(GDK_PIXBUF_LIBS) $(GL_LIBS) $(EGL_LIBS) -ldl -lrt
	@echo "==> Done: $@"

$(OBJ_DIR)/vulkan/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(VULKAN_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/vulkan/$(IMGUI_DIR)/backends/imgui_impl_vulkan.o: \
        $(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(VULKAN_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) \
	    -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES $(DEPFLAGS) -c $< -o $@

ifeq ($(ARCH),i386)
$(OBJ_DIR)/$(ARCH)/src/I386DsoHandle.o: src/I386DsoHandle.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(SHARED_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(ARCH_STUB_LIB):
	@mkdir -p $(dir $@)
	$(AR) rcs $@
endif

# OpenGL renderer
$(BIN_DIR)/lymalink-overlay-opengl.so: $(OPENGL_OBJECTS) $(ARCH_STUB_LIB) | $(BIN_DIR)
		@echo "==> Linking lymalink-overlay-opengl.so..."
	$(CXX) $(SHARED_CXXFLAGS) $(ARCH_LDFLAGS) -shared -o $@ $^ \
		-Wl,--no-undefined $(GDK_PIXBUF_LIBS) $(GL_LIBS) $(EGL_LIBS) -ldl -lrt
	@echo "==> Done: $@"

$(OBJ_DIR)/opengl/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(OPENGL_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# OpenGL preloader
$(BIN_DIR)/lymalink-overlay-preloader.so: $(PRELOADER_OBJECTS) $(ARCH_STUB_LIB) | $(BIN_DIR)
	@echo "==> Linking lymalink-overlay-preloader.so..."
	$(CXX) $(SHARED_CXXFLAGS) $(ARCH_LDFLAGS) -shared -o $@ $^ \
	    -Wl,--no-undefined -ldl
	@echo "==> Done: $@"

$(OBJ_DIR)/preloader/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(SHARED_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_ROOT)

overlay: all

-include $(ALL_DEPS)
