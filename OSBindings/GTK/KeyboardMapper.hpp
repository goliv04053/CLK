//
//  KeyboardMapper.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include "../../Inputs/Keyboard.hpp"

#include <optional>

namespace GTKFrontend {

/// Maps GDK hardware keycodes to Clock Signal's idealised keyboard by physical position,
/// so the mapping is independent of the host's keyboard layout. On Linux, under both
/// X11 and Wayland, GDK reports evdev scancodes offset by 8.
std::optional<Inputs::Keyboard::Key> key_for_keycode(unsigned int keycode);

}
