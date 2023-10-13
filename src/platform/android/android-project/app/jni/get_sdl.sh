#!/usr/bin/env sh

set -eu
if [ ! -d "$1"/SDL2 ] ; then
    git -C "$1" clone --quiet --depth 1 --branch "$2" https://github.com/libsdl-org/SDL.git SDL2
    find "$1"/SDL2 -mindepth 1 -maxdepth 1 ! -name 'Android.mk' ! -name 'include' ! -name 'src' -exec rm -rf {} \;
fi
