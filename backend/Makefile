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
TEST_TARGET := SQLiteManagerTests
BUILD_DIR := $(BUILD_ROOT)/$(BUILD)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TEST_BIN_DIR := $(BUILD_DIR)/tests

CPPFLAGS := -Isrc -I../frontend/src
CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -pedantic
CXXFLAGS_debug := -O0 -g
CXXFLAGS_release := -O2 -DNDEBUG
CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_$(BUILD))
CATCH2_LIBS := $(shell pkg-config --exists catch2-with-main 2>/dev/null && pkg-config --libs catch2-with-main)
CANBERRA_CFLAGS := $(shell pkg-config --cflags libcanberra 2>/dev/null)
CANBERRA_LIBS := $(shell pkg-config --libs libcanberra 2>/dev/null || echo -lcanberra)
SDBUS_LIBS := $(shell pkg-config --libs sdbus-c++)

SOURCES := \
    src/main.cpp \
    src/Lymalinkd.cpp \
    src/tools/Utils.cpp \
    src/tools/Logger.cpp \
    src/service/SystemdNotify.cpp \
    src/notification/AchievementNotificationService.cpp \
    src/notification/CanberraSoundService.cpp \
    src/notification/FreedesktopNotificationService.cpp \
    src/database/SQLiteManager.cpp \
    src/ipc/DBusService.cpp \
    src/watcher/PathScanner.cpp \
    src/watcher/ProcessWatcher.cpp

OBJECTS := $(SOURCES:src/%.cpp=$(OBJ_DIR)/%.o)

TEST_SOURCES := \
    tests/SQLiteManagerTests.cpp \
    src/database/SQLiteManager.cpp

.PHONY: all clean test

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(SDBUS_LIBS) $(CANBERRA_LIBS) -lsqlite3

$(TEST_BIN_DIR)/$(TEST_TARGET): $(TEST_SOURCES) | $(TEST_BIN_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(CATCH2_LIBS) -lsqlite3

test: $(TEST_BIN_DIR)/$(TEST_TARGET)
	$(TEST_BIN_DIR)/$(TEST_TARGET)

$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CANBERRA_CFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR) $(TEST_BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_ROOT)
