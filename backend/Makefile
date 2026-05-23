#########################################################
# File: Makefile
# Date: 22.05.2026
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Makefile for Lymalinkd backend service
#########################################################

CXX ?= g++
BUILD ?= debug
BUILD_ROOT ?= build

TARGET := lymalinkd
BUILD_DIR := $(BUILD_ROOT)/$(BUILD)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

CPPFLAGS := -Isrc -I../frontend/src
CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -pedantic
CXXFLAGS_debug := -O0 -g
CXXFLAGS_release := -O2 -DNDEBUG
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_$(BUILD))

SOURCES := \
    src/main.cpp \
    src/Lymalinkd.cpp \
    src/tools/Logger.cpp \
    src/service/SystemdNotify.cpp \
    src/database/SQLiteManager.cpp \
    src/ipc/DBusService.cpp \
    src/watcher/PathScanner.cpp \
    src/watcher/ProcessWatcher.cpp

OBJECTS := $(SOURCES:src/%.cpp=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(shell pkg-config --libs sdbus-c++)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_ROOT)
