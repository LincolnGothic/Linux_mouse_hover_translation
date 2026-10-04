# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
# Source this file; activation affects the current shell only.
export HOVER_TOOLS_ROOT=${HOVER_TOOLS_ROOT:-/workspace/.hover-translation-tools}
export HOVER_SDK="$HOVER_TOOLS_ROOT/sysroot"
export PATH="$HOVER_SDK/usr/bin:$PATH"
export LD_LIBRARY_PATH="$HOVER_SDK/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PKG_CONFIG_PATH="$HOVER_SDK/usr/lib/x86_64-linux-gnu/pkgconfig:$HOVER_SDK/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_SYSROOT_DIR="$HOVER_SDK"
export CMAKE_PREFIX_PATH="$HOVER_SDK/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
export QT_PLUGIN_PATH="$HOVER_SDK/usr/lib/x86_64-linux-gnu/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export TESSDATA_PREFIX="$HOVER_SDK/usr/share/tesseract-ocr/5/tessdata"
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-$HOVER_TOOLS_ROOT/cache}
mkdir -p "$XDG_CACHE_HOME"
