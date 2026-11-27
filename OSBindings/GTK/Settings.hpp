//
//  Settings.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include <optional>
#include <string>

namespace GTKFrontend::Settings {

// The frontend's equivalent of the Mac version's user defaults: a key file at
// ~/.config/clksignal/gtk.ini. A key file needs no schema to be installed, which
// keeps the AppImage self-contained. Groups and keys are plain ASCII identifiers.

std::optional<int> get_int(const char *group, const std::string &key);
std::optional<std::string> get_string(const char *group, const std::string &key);

void set_int(const char *group, const std::string &key, int value);
void set_string(const char *group, const std::string &key, const std::string &value);

/// Writes any changes to disk.
void save();

}
