//
//  KeyboardMapper.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "KeyboardMapper.hpp"

#include <linux/input-event-codes.h>

#include <unordered_map>

namespace GTKFrontend {

std::optional<Inputs::Keyboard::Key> key_for_keycode(const unsigned int keycode) {
	using Key = Inputs::Keyboard::Key;
	static const std::unordered_map<unsigned int, Key> map = {
		{KEY_ESC, Key::Escape},
		{KEY_F1, Key::F1}, {KEY_F2, Key::F2}, {KEY_F3, Key::F3}, {KEY_F4, Key::F4},
		{KEY_F5, Key::F5}, {KEY_F6, Key::F6}, {KEY_F7, Key::F7}, {KEY_F8, Key::F8},
		{KEY_F9, Key::F9}, {KEY_F10, Key::F10}, {KEY_F11, Key::F11}, {KEY_F12, Key::F12},
		{KEY_SYSRQ, Key::PrintScreen}, {KEY_SCROLLLOCK, Key::ScrollLock}, {KEY_PAUSE, Key::Pause},

		{KEY_GRAVE, Key::BackTick},
		{KEY_1, Key::k1}, {KEY_2, Key::k2}, {KEY_3, Key::k3}, {KEY_4, Key::k4}, {KEY_5, Key::k5},
		{KEY_6, Key::k6}, {KEY_7, Key::k7}, {KEY_8, Key::k8}, {KEY_9, Key::k9}, {KEY_0, Key::k0},
		{KEY_MINUS, Key::Hyphen}, {KEY_EQUAL, Key::Equals}, {KEY_BACKSPACE, Key::Backspace},

		{KEY_TAB, Key::Tab},
		{KEY_Q, Key::Q}, {KEY_W, Key::W}, {KEY_E, Key::E}, {KEY_R, Key::R}, {KEY_T, Key::T},
		{KEY_Y, Key::Y}, {KEY_U, Key::U}, {KEY_I, Key::I}, {KEY_O, Key::O}, {KEY_P, Key::P},
		{KEY_LEFTBRACE, Key::OpenSquareBracket}, {KEY_RIGHTBRACE, Key::CloseSquareBracket},
		{KEY_BACKSLASH, Key::Backslash},

		{KEY_CAPSLOCK, Key::CapsLock},
		{KEY_A, Key::A}, {KEY_S, Key::S}, {KEY_D, Key::D}, {KEY_F, Key::F}, {KEY_G, Key::G},
		{KEY_H, Key::H}, {KEY_J, Key::J}, {KEY_K, Key::K}, {KEY_L, Key::L},
		{KEY_SEMICOLON, Key::Semicolon}, {KEY_APOSTROPHE, Key::Quote},
		{KEY_ENTER, Key::Enter},

		// ISO keyboards: the key between left shift and Z; the Mac and Qt ports treat it as backslash.
		{KEY_102ND, Key::Backslash},

		{KEY_LEFTSHIFT, Key::LeftShift},
		{KEY_Z, Key::Z}, {KEY_X, Key::X}, {KEY_C, Key::C}, {KEY_V, Key::V}, {KEY_B, Key::B},
		{KEY_N, Key::N}, {KEY_M, Key::M},
		{KEY_COMMA, Key::Comma}, {KEY_DOT, Key::FullStop}, {KEY_SLASH, Key::ForwardSlash},
		{KEY_RIGHTSHIFT, Key::RightShift},
		// Brazilian ABNT2: the extra /? key next to right shift.
		{KEY_RO, Key::ForwardSlash},

		{KEY_LEFTCTRL, Key::LeftControl}, {KEY_LEFTALT, Key::LeftOption}, {KEY_LEFTMETA, Key::LeftMeta},
		{KEY_SPACE, Key::Space},
		{KEY_RIGHTMETA, Key::RightMeta}, {KEY_RIGHTALT, Key::RightOption}, {KEY_RIGHTCTRL, Key::RightControl},

		{KEY_LEFT, Key::Left}, {KEY_RIGHT, Key::Right}, {KEY_UP, Key::Up}, {KEY_DOWN, Key::Down},

		{KEY_INSERT, Key::Insert}, {KEY_HOME, Key::Home}, {KEY_PAGEUP, Key::PageUp},
		{KEY_DELETE, Key::Delete}, {KEY_END, Key::End}, {KEY_PAGEDOWN, Key::PageDown},

		{KEY_NUMLOCK, Key::NumLock}, {KEY_KPSLASH, Key::KeypadSlash},
		{KEY_KPASTERISK, Key::KeypadAsterisk}, {KEY_KPMINUS, Key::KeypadMinus},
		{KEY_KP7, Key::Keypad7}, {KEY_KP8, Key::Keypad8}, {KEY_KP9, Key::Keypad9}, {KEY_KPPLUS, Key::KeypadPlus},
		{KEY_KP4, Key::Keypad4}, {KEY_KP5, Key::Keypad5}, {KEY_KP6, Key::Keypad6},
		{KEY_KP1, Key::Keypad1}, {KEY_KP2, Key::Keypad2}, {KEY_KP3, Key::Keypad3}, {KEY_KPENTER, Key::KeypadEnter},
		{KEY_KP0, Key::Keypad0}, {KEY_KPDOT, Key::KeypadDecimalPoint}, {KEY_KPEQUAL, Key::KeypadEquals},
		{KEY_KPCOMMA, Key::KeypadDecimalPoint},

		{KEY_HELP, Key::Help},
	};

	// GDK's hardware keycodes are evdev codes plus 8 (the X11 convention, also used under Wayland).
	if(keycode < 8) return std::nullopt;
	const auto found = map.find(keycode - 8);
	if(found == map.end()) return std::nullopt;
	return found->second;
}

}
