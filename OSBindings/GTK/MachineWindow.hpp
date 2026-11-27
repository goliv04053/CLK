//
//  MachineWindow.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include <adwaita.h>

#include "AudioOutput.hpp"
#include "MachineRunner.hpp"

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"
#include "../../Machines/Utility/ROMCatalogue.hpp"
#include "../../Outputs/OpenGL/ScanTarget.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace GTKFrontend {

/// One window per emulated machine, in the manner of the Mac version's documents.
/// The window owns itself: it is destroyed when GTK destroys the window.
class MachineWindow {
public:
	/// Creates and presents a new window, optionally opening @c file in it.
	static MachineWindow *create(GtkApplication *, GFile *file = nullptr);

	/// @returns the MachineWindow for @c window, if it is one.
	static MachineWindow *from(GtkWindow *window);

	/// Opens a file: into this window if it is idle, or a new window otherwise.
	void open(GFile *file);

	bool has_machine() const { return bool(machine_); }
	GtkWindow *window() const { return GTK_WINDOW(window_); }

private:
	explicit MachineWindow(GtkApplication *);
	~MachineWindow();

	enum class Phase {
		Welcome, RequestingROMs, Running
	} phase_ = Phase::Welcome;
	void set_phase(Phase);

	// Machine lifecycle.
	bool launch_file(const std::filesystem::path &);
	void launch_targets();
	void delete_machine();
	bool insert_file(const std::filesystem::path &);
	void update_title();
	void update_actions();

	// Input.
	bool key_event(unsigned keyval, unsigned keycode, GdkModifierType state, bool pressed);
	void release_all_keys();
	std::set<unsigned> pressed_keycodes_;
	bool keyboard_as_joystick_ = false;

	// Display.
	void gl_realize();
	void gl_unrealize();
	bool gl_render();

	// File handling.
	bool handle_drop(const GSList *files);
	/// Installs any ROMs among @c files, or within folders among them, with feedback.
	/// @returns @c true if any ROM was installed.
	bool install_roms(const std::vector<std::filesystem::path> &files, bool report_unrecognised = true);
	void choose_file(bool insert);
	void choose_roms();
	void open_rom_folder();
	void populate_rom_list();
	void show_toast(const std::string &);

	// Widgets.
	AdwApplicationWindow *window_ = nullptr;
	AdwToolbarView *toolbar_view_ = nullptr;
	AdwWindowTitle *title_ = nullptr;
	AdwToastOverlay *toasts_ = nullptr;
	GtkStack *stack_ = nullptr;
	AdwStatusPage *roms_page_ = nullptr;
	GtkBox *roms_list_ = nullptr;
	GtkGLArea *gl_area_ = nullptr;
	GSimpleActionGroup *actions_ = nullptr;

	// Emulation state.
	Analyser::Static::TargetList targets_;
	std::string display_name_;
	std::filesystem::path source_file_;
	ROM::Request missing_roms_;
	std::unique_ptr<Machine::DynamicMachine> machine_;
	std::mutex machine_mutex_;
	MachineRunner runner_;
	AudioOutput audio_;

	std::unique_ptr<Outputs::Display::OpenGL::ScanTarget> scan_target_;
	MachineTypes::ScanProducer *pending_producer_ = nullptr;
	Outputs::Display::OpenGL::API api_ = Outputs::Display::OpenGL::API::OpenGL32Core;
	GLuint framebuffer_ = 0;
	guint tick_id_ = 0;
};

}
