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
BUILD ?= debug
BUILD_ROOT ?= build

BUILD_DIR := $(BUILD_ROOT)/$(BUILD)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

IMGUI_DIR ?= src/imgui
IMGUI_CFLAGS := -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends

CPPFLAGS := -Iinclude -Isrc -I../backend/src
CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -pedantic
CXXFLAGS_debug := -O0 -g
CXXFLAGS_release := -O2 -DNDEBUG
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_$(BUILD))
DEPFLAGS := -MMD -MP

SHARED_CXXFLAGS := $(CXXFLAGS) -fPIC -fvisibility=hidden
IMGUI_BUILD_CXXFLAGS := $(SHARED_CXXFLAGS) $(IMGUI_CFLAGS)
OPENGL_CFLAGS := -DLYMALINK_OVERLAY_OPENGL_TEXTURES

VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
VULKAN_LIBS := $(shell pkg-config --libs   vulkan 2>/dev/null || echo -lvulkan)
GDK_PIXBUF_CFLAGS := $(shell pkg-config --cflags gdk-pixbuf-2.0 2>/dev/null)
GDK_PIXBUF_LIBS := $(shell pkg-config --libs gdk-pixbuf-2.0 2>/dev/null || echo -lgdk_pixbuf-2.0)

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
ALL_OBJECTS := $(VULKAN_OBJECTS) $(OPENGL_OBJECTS) $(PRELOADER_OBJECTS)
ALL_DEPS := $(ALL_OBJECTS:.o=.d)

.PHONY: all clean overlay

all: \
    $(BIN_DIR)/lymalink-overlay.so \
    $(BIN_DIR)/lymalink-overlay-opengl.so \
    $(BIN_DIR)/lymalink-overlay-preloader.so

# Vulkan layer
$(BIN_DIR)/lymalink-overlay.so: $(VULKAN_OBJECTS) | $(BIN_DIR)
	@echo "==> Linking lymalink-overlay.so..."
	$(CXX) $(SHARED_CXXFLAGS) -shared -o $@ $^ \
		-Wl,--no-undefined $(VULKAN_LIBS) $(GDK_PIXBUF_LIBS) -lGL -lEGL -ldl -lrt
	@echo "==> Done: $@"

$(OBJ_DIR)/vulkan/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(VULKAN_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/vulkan/$(IMGUI_DIR)/backends/imgui_impl_vulkan.o: \
        $(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(VULKAN_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) \
	    -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES $(DEPFLAGS) -c $< -o $@

# OpenGL renderer
$(BIN_DIR)/lymalink-overlay-opengl.so: $(OPENGL_OBJECTS) | $(BIN_DIR)
	@echo "==> Linking lymalink-overlay-opengl.so..."
	$(CXX) $(SHARED_CXXFLAGS) -shared -o $@ $^ \
		-Wl,--no-undefined $(GDK_PIXBUF_LIBS) -lGL -lEGL -ldl -lrt
	@echo "==> Done: $@"

$(OBJ_DIR)/opengl/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(OPENGL_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(IMGUI_BUILD_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# OpenGL preloader
$(BIN_DIR)/lymalink-overlay-preloader.so: $(PRELOADER_OBJECTS) | $(BIN_DIR)
	@echo "==> Linking lymalink-overlay-preloader.so..."
	$(CXX) $(SHARED_CXXFLAGS) -shared -o $@ $^ \
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
