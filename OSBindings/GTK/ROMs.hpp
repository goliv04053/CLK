//
//  ROMs.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include "../../Machines/ROMMachine.hpp"
#include "../../Machines/Utility/ROMCatalogue.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace GTKFrontend::ROMs {

/// All folders searched for ROM images, in priority order. Each contains one
/// subfolder per machine, matching the layout of the repository's ROMImages folder.
/// The first entry is the user's writeable folder.
std::vector<std::filesystem::path> search_paths();

/// The writeable folder into which user-supplied ROMs are copied.
std::filesystem::path user_path();

/// Builds a ROM fetcher that searches all search paths and then the ROMs built into
/// Clock Signal; anything still missing is written to @c missing.
ROMMachine::ROMFetcher fetcher(ROM::Request &missing);

/// Outcome of trying to install a single file as a ROM.
struct InstallResult {
	bool installed = false;
	ROM::Name name = ROM::Name::None;	// Set if installed.
	std::string descriptive_name;		// Set if installed.
};

/// Identifies @c file by CRC and, if it is a known ROM, copies it into the user's ROM folder.
/// As per the Mac version, a file that is longer than a known ROM but begins with it is also accepted.
InstallResult install(const std::filesystem::path &file);

/// @returns @c true if @c file is exactly a ROM image that Clock Signal knows, by CRC.
bool is_known_rom(const std::filesystem::path &file);

/// Expands any folders within @c paths into the files they contain, recursively, so that a whole
/// ROM collection can be dropped at once. Files too large to be ROMs are skipped.
std::vector<std::filesystem::path> expand_folders(const std::vector<std::filesystem::path> &paths);

/// @returns @c path for display, with the home folder shown as ~.
std::string display_path(const std::filesystem::path &path);

/// Converts a wide string, as produced by ROM::Request::description, to UTF-8.
std::string to_utf8(const std::wstring &);

}
