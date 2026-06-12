#!/usr/bin/env bash
#########################################################
# File: lymalink-overlay.sh
# Date: 2026-05-28
# Author: Morsomus
# Copyright: see /LICENSE
# Description: Launcher script for OpenGL overlay preloading
#              for Steam and Wine
#
#   Steam launch option: lymalink-overlay %command%
#   Wine launching: lymalink-overlay wine executable.exe
#########################################################

set -e

##############################################################################

# Resolve the correct home directory, handling special cases like Snap packages
resolve_login_home() {
    local login_home=""

    # Try to fetch the official home directory from the system passwd database
    if command -v getent >/dev/null 2>&1 && [ -n "${USER:-}" ]; then
        login_home="$(getent passwd "$USER" | cut -d: -f6)"
    fi

    # If getent returned a path different from $HOME, check for Snap overrides
    if [ -n "$login_home" ] && [ "$HOME" != "$login_home" ]; then
        case "$HOME" in
            "$login_home"/snap/*) printf '%s\n' "$login_home"; return ;;
        esac
    fi

    # Default to $HOME if no override was detected
    printf '%s\n' "$HOME"
}

##############################################################################

add_preload() {
    local lib="$1"

    case ":${LD_PRELOAD-}:" in
        (*:"$lib":*) ;;
        (*)
            if [ -n "${LD_PRELOAD-}" ]; then
                export LD_PRELOAD="${LD_PRELOAD}:$lib"
            else
                export LD_PRELOAD="$lib"
            fi
            ;;
    esac
}

##############################################################################

LOGIN_HOME="$(resolve_login_home)"
LIB_DIR="${LYMALINK_OVERLAY_LIB_DIR:-$LOGIN_HOME/.local/lib}"

# Set paths for both architectures
PRELOADER_64="$LIB_DIR/lymalink-overlay-preloader.so"
PRELOADER_32="$LIB_DIR/i386-linux-gnu/lymalink-overlay-preloader.so"
  
# log_error() {
#     local timestamp
#     timestamp="$(date '+%a %b %d %H:%M:%S %Y')"
#     printf '%s pid=%s [lymalink-overlay] %s\n' "$timestamp" "$$" "$1" >> /tmp/lymalink-overlay.log
#     printf '%s pid=%s [lymalink-overlay] %s\n' "$timestamp" "$$" "$1" >&2
# }

# Ensure at least one command argument is provided
if [ "$#" -eq 0 ]; then
    # log_error "no command supplied"
    exit 1
fi

# Dynamically add readable preload libraries
PRELOADER_FOUND=0

if [ -r "$PRELOADER_64" ]; then
    add_preload "$PRELOADER_64"
    # log_error "loader selected: $PRELOADER_64"
    PRELOADER_FOUND=1
fi

if [ -r "$PRELOADER_32" ]; then
    add_preload "$PRELOADER_32"
    # log_error "loader selected: $PRELOADER_32"
    PRELOADER_FOUND=1
fi

# Exit with an error if neither library was found
if [ "$PRELOADER_FOUND" -eq 0 ]; then
    # log_error "preloader libraries not readable or missing"
    exit 1
fi

# Execute the provided command with the modified environment
exec "$@"
