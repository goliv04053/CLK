//
//  ROMs.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "ROMs.hpp"

#include "../../Machines/Utility/ROMLibrary.hpp"
#include "../../Numeric/CRC.hpp"

#include <glib.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>
#include <unistd.h>

namespace GTKFrontend::ROMs {
namespace {

std::optional<std::vector<uint8_t>> read_file(const std::filesystem::path &path) {
	std::ifstream stream(path, std::ios::binary);
	if(!stream) return std::nullopt;
	std::vector<uint8_t> contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	if(stream.bad()) return std::nullopt;
	return contents;
}

std::filesystem::path executable_directory() {
	std::error_code error;
	const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::filesystem::path() : path.parent_path();
}

}

std::filesystem::path user_path() {
	// Same folder the Qt frontend uses, so existing ROM collections keep working.
	return std::filesystem::path(g_get_user_data_dir()) / "clksignal" / "ROMImages";
}

std::vector<std::filesystem::path> search_paths() {
	std::vector<std::filesystem::path> paths;
	paths.push_back(user_path());

	// An AppImage, Flatpak or system install may ship ROMs alongside the binary.
	if(const char *const appdir = std::getenv("APPDIR")) {
		paths.push_back(std::filesystem::path(appdir) / "usr/share/clksignal/ROMImages");
	}
	const auto exe = executable_directory();
	if(!exe.empty()) {
		paths.push_back(exe / "../share/clksignal/ROMImages");
	}
	for(const gchar *const *dir = g_get_system_data_dirs(); *dir; ++dir) {
		paths.push_back(std::filesystem::path(*dir) / "clksignal" / "ROMImages");
	}
	return paths;
}

ROMMachine::ROMFetcher fetcher(ROM::Request &missing) {
	return [&missing](const ROM::Request &request) -> ROM::Map {
		ROM::Map results;
		const auto paths = search_paths();

		for(const auto &description: request.all_descriptions()) {
			bool found = false;
			for(const auto &base: paths) {
				for(const auto &file_name: description.file_names) {
					auto data = read_file(base / description.machine_name / file_name);
					if(data) {
						results[description.name] = std::move(*data);
						found = true;
						break;
					}
				}
				if(found) break;
			}

			if(!found) {
				auto data = ROM::included_rom_image(description.name);
				if(data) {
					results[description.name] = std::move(*data);
				}
			}
		}

		missing = request.subtract(results);
		return results;
	};
}

namespace {
// Larger than any ROM in the catalogue; anything bigger isn't worth reading.
constexpr std::uintmax_t MaximumROMSize = 16 * 1024 * 1024;
}

InstallResult install(const std::filesystem::path &file) {
	InstallResult result;
	std::error_code error;
	const auto file_size = std::filesystem::file_size(file, error);
	if(error || !file_size || file_size > MaximumROMSize) return result;

	auto contents = read_file(file);
	if(!contents || contents->empty()) return result;

	// Try for a direct CRC match.
	auto description = ROM::Description::from_crc(CRC::CRC32::crc_of(*contents));

	// Failing that, see whether there's an acceptable trimming that creates a match,
	// e.g. a dump with a header or trailing padding.
	if(!description) {
		for(const auto &candidate: ROM::all_descriptions()) {
			if(!candidate.size || candidate.size >= contents->size()) continue;
			const std::vector<uint8_t> trimmed(contents->begin(), contents->begin() + ptrdiff_t(candidate.size));
			if(candidate.crc32s.contains(CRC::CRC32::crc_of(trimmed))) {
				description = candidate;
				contents->resize(candidate.size);
				break;
			}
		}
	}
	if(!description || description->file_names.empty()) return result;

	const auto folder = user_path() / description->machine_name;
	error.clear();
	std::filesystem::create_directories(folder, error);
	if(error) return result;

	std::ofstream output(folder / description->file_names[0], std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char *>(contents->data()), std::streamsize(contents->size()));
	if(!output) return result;

	result.installed = true;
	result.name = description->name;
	result.descriptive_name = description->descriptive_name;
	return result;
}

bool is_known_rom(const std::filesystem::path &file) {
	std::error_code error;
	const auto file_size = std::filesystem::file_size(file, error);
	if(error || !file_size || file_size > MaximumROMSize) return false;

	const auto contents = read_file(file);
	return contents && ROM::Description::from_crc(CRC::CRC32::crc_of(*contents)).has_value();
}

std::vector<std::filesystem::path> expand_folders(const std::vector<std::filesystem::path> &paths) {
	constexpr size_t MaximumFiles = 20'000;
	std::vector<std::filesystem::path> files;
	for(const auto &path: paths) {
		std::error_code error;
		if(!std::filesystem::is_directory(path, error)) {
			files.push_back(path);
			continue;
		}

		auto iterator = std::filesystem::recursive_directory_iterator(
			path, std::filesystem::directory_options::skip_permission_denied, error);
		for(; !error && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
			std::error_code entry_error;
			if(!iterator->is_regular_file(entry_error)) continue;
			const auto size = iterator->file_size(entry_error);
			if(entry_error || !size || size > MaximumROMSize) continue;
			files.push_back(iterator->path());
			if(files.size() >= MaximumFiles) return files;
		}
	}
	return files;
}

std::string display_path(const std::filesystem::path &path) {
	const std::string home = g_get_home_dir();
	const std::string text = path.string();
	if(!home.empty() && text.compare(0, home.size(), home) == 0) {
		return "~" + text.substr(home.size());
	}
	return text;
}

std::string to_utf8(const std::wstring &string) {
	std::string result;
	for(const wchar_t c: string) {
		gchar buffer[6];
		const auto length = g_unichar_to_utf8(gunichar(c), buffer);
		result.append(buffer, size_t(length));
	}
	return result;
}

}
