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

PRELOADER_LIB="${LYMALINK_OVERLAY_LIB_DIR:-$HOME/.local/lib}/lymalink-overlay-preloader.so"

# log_error() {
#     local timestamp
#     timestamp="$(date '+%a %b %d %H:%M:%S %Y')"
#     printf '%s pid=%s [lymalink-overlay] %s\n' "$timestamp" "$$" "$1" >> /tmp/lymalink-overlay.log
#     printf '%s pid=%s [lymalink-overlay] %s\n' "$timestamp" "$$" "$1" >&2
# }

if [ "$#" -eq 0 ]; then
    # log_error "no command supplied"
    exit 1
fi

if [ ! -r "$PRELOADER_LIB" ]; then
    # log_error "preloader library not readable: $PRELOADER_LIB"
    exit 1
fi

export LYMALINK_OVERLAY=1

case ":${LD_PRELOAD-}:" in
    (*:"$PRELOADER_LIB":*) ;;
    (*)
        if [ -n "${LD_PRELOAD-}" ]; then
            export LD_PRELOAD="${LD_PRELOAD}:$PRELOADER_LIB"
        else
            export LD_PRELOAD="$PRELOADER_LIB"
        fi
        ;;
esac

exec "$@"
