#!/bin/sh
# Builds ClockSignal-x86_64.AppImage with the GTK4/libadwaita frontend.
# Needs: cmake, ninja, a C++20 compiler, gtk4 + libadwaita (>= 1.5) + libpulse + zlib development
# packages, mksquashfs (squashfs-tools) and the AppImage type2 runtime
# (https://github.com/AppImage/type2-runtime/releases, file "runtime-x86_64").
#
# Usage: OSBindings/GTK/packaging/build-appimage.sh path/to/runtime-x86_64 [build-dir]
set -e
RUNTIME="$(readlink -f "${1:?usage: $0 path/to/runtime-x86_64 [build-dir]}")"
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="$(readlink -f "${2:-$ROOT/build-gtk}")"
APP_ID="${CLK_GTK_APP_ID:-io.github.tomharte.ClockSignal}"

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCLK_UI=GTK -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCLK_GTK_APP_ID="$APP_ID"
cmake --build "$BUILD"

A="$BUILD/AppDir"
rm -rf "$A"
DESTDIR="$A" cmake --install "$BUILD"
strip "$A/usr/bin/clksignal"

# Nothing is bundled: GTK4, libadwaita, OpenGL and PulseAudio (served by PipeWire) are part of any
# current GNOME desktop, and using the host's copies keeps the app in line with the system theme.

cp "$ROOT/OSBindings/GTK/packaging/AppRun" "$A/AppRun"
cp "$A/usr/share/applications/$APP_ID.desktop" "$A/"
cp "$A/usr/share/icons/hicolor/512x512/apps/$APP_ID.png" "$A/"
ln -sf "$APP_ID.png" "$A/.DirIcon"
mkdir -p "$A/usr/share/doc/clksignal"
cp "$ROOT/LICENCE" "$A/usr/share/doc/clksignal/"

OUT="$ROOT/ClockSignal-x86_64.AppImage"
rm -f "$BUILD/clksignal.squashfs" "$OUT"
mksquashfs "$A" "$BUILD/clksignal.squashfs" -root-owned -noappend -comp zstd -quiet
cat "$RUNTIME" "$BUILD/clksignal.squashfs" > "$OUT"
chmod +x "$OUT"
echo "OK: $OUT"
