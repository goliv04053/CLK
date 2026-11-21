# Clock Signal: GTK4/libadwaita frontend (unofficial)

A native GNOME frontend for [Clock Signal](https://github.com/TomHarte/CLK), the emulator by
Thomas Harte. It uses the same C++ emulation core and OpenGL scan target as the other frontends,
and aims for feature parity with the macOS version; see [PARIDADE.md](PARIDADE.md) for the
checklist (in Portuguese).

This frontend is not part of the upstream project. Please report problems with it at
https://github.com/goliv04053/CLK/issues rather than upstream.

## Building

Fedora:

    sudo dnf install cmake ninja-build gcc-c++ gtk4-devel libadwaita-devel pulseaudio-libs-devel zlib-devel gettext
    cmake -S . -B build-gtk -G Ninja -DCLK_UI=GTK -DCMAKE_BUILD_TYPE=Release
    cmake --build build-gtk
    ./build-gtk/clksignal [file]

Debian/Ubuntu: `libgtk-4-dev libadwaita-1-dev libpulse-dev zlib1g-dev gettext`.

Requires GTK 4.12 or later and libadwaita 1.5 or later.

AppImage: `OSBindings/GTK/packaging/build-appimage.sh path/to/runtime-x86_64`, using the runtime from
https://github.com/AppImage/type2-runtime/releases.

## ROMs

Clock Signal has the Amstrad CPC, ZX Spectrum and ZX80/81 ROMs built in. Other machines need ROM
images; when one is missing the window lists what's needed, and you can drop the files onto it or
choose them. They're copied to `~/.local/share/clksignal/ROMImages/<machine>/`, the same folder the
Qt frontend uses.

## Keyboard

The keyboard is mapped by physical position. While a machine is running, Ctrl belongs to the
machine, so the window's shortcuts also work with Ctrl+Shift: Ctrl+Shift+O to open, Ctrl+Shift+I to
insert media, Ctrl+Shift+R / Ctrl+Shift+H for soft / hard reset, F11 or Ctrl+Shift+F for fullscreen,
Ctrl+Shift+W to close.

## Layout

All frontend code lives in this folder, so that merging upstream changes stays easy:

- `main.cpp`: the application, its actions and shortcuts, the About dialog;
- `MachineWindow.*`: one window per machine — welcome page, ROM requester, display, input;
- `MachineRunner.*`: runs the machine in real time on its own thread;
- `AudioOutput.*`: audio via PulseAudio (PipeWire on current desktops);
- `KeyboardMapper.*`: GDK keycodes to Clock Signal keys;
- `ROMs.*`: ROM search paths, fetching and installation;
- `po/`: translations (pt_BR, es); `data/`: desktop file, AppStream metadata, icon;
  `packaging/`: AppImage build.

## Credits

Clock Signal is © Thomas Harte, under the MIT licence (see `LICENCE`). This frontend was written by
Claude (Anthropic), at the request of Gabriel ([@goliv04053](https://github.com/goliv04053)), who
also tests it; it reuses ideas and code from the Qt and SDL frontends and follows the macOS version's design.
