//
//  Settings.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "Settings.hpp"

#include <glib.h>
#include <glib/gstdio.h>

#include <filesystem>

namespace GTKFrontend::Settings {
namespace {

std::filesystem::path path() {
	return std::filesystem::path(g_get_user_config_dir()) / "clksignal" / "gtk.ini";
}

GKeyFile *file() {
	static GKeyFile *const key_file = [] {
		GKeyFile *const key_file = g_key_file_new();
		g_key_file_load_from_file(key_file, path().c_str(), G_KEY_FILE_KEEP_COMMENTS, nullptr);
		return key_file;
	}();
	return key_file;
}

}

std::optional<int> get_int(const char *const group, const std::string &key) {
	GError *error = nullptr;
	const int value = g_key_file_get_integer(file(), group, key.c_str(), &error);
	if(error) {
		g_error_free(error);
		return std::nullopt;
	}
	return value;
}

std::optional<std::string> get_string(const char *const group, const std::string &key) {
	gchar *const value = g_key_file_get_string(file(), group, key.c_str(), nullptr);
	if(!value) return std::nullopt;
	std::string result = value;
	g_free(value);
	return result;
}

void set_int(const char *const group, const std::string &key, const int value) {
	g_key_file_set_integer(file(), group, key.c_str(), value);
}

void set_string(const char *const group, const std::string &key, const std::string &value) {
	g_key_file_set_string(file(), group, key.c_str(), value.c_str());
}

void save() {
	const auto target = path();
	g_mkdir_with_parents(target.parent_path().c_str(), 0700);
	g_key_file_save_to_file(file(), target.c_str(), nullptr);
}

}
