//
//  MachineWindow.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "MachineWindow.hpp"

#include "i18n.hpp"
#include "KeyboardMapper.hpp"
#include "ROMs.hpp"

#include "../../Configurable/StandardOptions.hpp"
#include "../../Outputs/OpenGL/Primitives/Shader.hpp"


using namespace GTKFrontend;

namespace {

constexpr const char *WindowKey = "clk-machine-window";

std::filesystem::path path_of(GFile *const file) {
	char *const path = g_file_get_path(file);
	if(!path) return {};
	std::filesystem::path result(path);
	g_free(path);
	return result;
}

}

// MARK: - Construction.

MachineWindow *MachineWindow::create(GtkApplication *const application, GFile *const file) {
	auto *const window = new MachineWindow(application);
	gtk_window_present(window->window());
	if(file) window->open(file);
	return window;
}

MachineWindow *MachineWindow::from(GtkWindow *const window) {
	return static_cast<MachineWindow *>(g_object_get_data(G_OBJECT(window), WindowKey));
}

MachineWindow::MachineWindow(GtkApplication *const application) {
	window_ = ADW_APPLICATION_WINDOW(adw_application_window_new(application));
	g_object_set_data(G_OBJECT(window_), WindowKey, this);
	gtk_window_set_default_size(GTK_WINDOW(window_), 800, 600);
	gtk_window_set_icon_name(GTK_WINDOW(window_), CLK_GTK_APP_ID);

	// Actions; these are this window's equivalent of the Mac version's document menu items.
	static const GActionEntry entries[] = {
		{"open", [](GSimpleAction *, GVariant *, gpointer self) {
			static_cast<MachineWindow *>(self)->choose_file(false);
		}, nullptr, nullptr, nullptr, {}},
		{"insert", [](GSimpleAction *, GVariant *, gpointer self) {
			static_cast<MachineWindow *>(self)->choose_file(true);
		}, nullptr, nullptr, nullptr, {}},
		{"choose-roms", [](GSimpleAction *, GVariant *, gpointer self) {
			static_cast<MachineWindow *>(self)->choose_roms();
		}, nullptr, nullptr, nullptr, {}},
		{"soft-reset", [](GSimpleAction *, GVariant *, gpointer data) {
			auto &self = *static_cast<MachineWindow *>(data);
			if(!self.machine_) return;
			std::lock_guard lock(self.machine_mutex_);
			if(auto *const resettable = self.machine_->soft_resettable()) resettable->soft_reset();
		}, nullptr, nullptr, nullptr, {}},
		{"hard-reset", [](GSimpleAction *, GVariant *, gpointer data) {
			auto &self = *static_cast<MachineWindow *>(data);
			if(!self.machine_) return;
			std::lock_guard lock(self.machine_mutex_);
			if(auto *const resettable = self.machine_->hard_resettable()) resettable->hard_reset();
		}, nullptr, nullptr, nullptr, {}},
		{"fullscreen", [](GSimpleAction *, GVariant *, gpointer data) {
			auto *const window = static_cast<MachineWindow *>(data)->window();
			if(gtk_window_is_fullscreen(window)) gtk_window_unfullscreen(window);
			else gtk_window_fullscreen(window);
		}, nullptr, nullptr, nullptr, {}},
		{"keyboard-as-joystick", nullptr, nullptr, "false", [](GSimpleAction *action, GVariant *value, gpointer data) {
			auto &self = *static_cast<MachineWindow *>(data);
			self.release_all_keys();
			self.keyboard_as_joystick_ = g_variant_get_boolean(value);
			g_simple_action_set_state(action, value);
		}, {}},
	};
	g_action_map_add_action_entries(G_ACTION_MAP(window_), entries, G_N_ELEMENTS(entries), this);

	// Header bar: an Open button and the primary menu, GNOME style.
	auto *const header = ADW_HEADER_BAR(adw_header_bar_new());
	title_ = ADW_WINDOW_TITLE(adw_window_title_new("Clock Signal", ""));
	adw_header_bar_set_title_widget(header, GTK_WIDGET(title_));

	auto *const open_button = gtk_button_new_from_icon_name("document-open-symbolic");
	gtk_widget_set_tooltip_text(open_button, _("Open…"));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(open_button), "win.open");
	adw_header_bar_pack_start(header, open_button);

	GMenu *const menu = g_menu_new();
	GMenu *const file_section = g_menu_new();
	g_menu_append(file_section, _("_New Window"), "app.new-window");
	g_menu_append(file_section, _("_Open…"), "win.open");
	g_menu_append(file_section, _("_Insert…"), "win.insert");
	g_menu_append_section(menu, nullptr, G_MENU_MODEL(file_section));
	GMenu *const machine_section = g_menu_new();
	g_menu_append(machine_section, _("_Soft Reset"), "win.soft-reset");
	g_menu_append(machine_section, _("_Hard Reset"), "win.hard-reset");
	g_menu_append(machine_section, _("Use Keyboard as _Joystick"), "win.keyboard-as-joystick");
	g_menu_append_section(menu, nullptr, G_MENU_MODEL(machine_section));
	GMenu *const view_section = g_menu_new();
	g_menu_append(view_section, _("_Fullscreen"), "win.fullscreen");
	g_menu_append_section(menu, nullptr, G_MENU_MODEL(view_section));
	GMenu *const app_section = g_menu_new();
	g_menu_append(app_section, _("_About Clock Signal"), "app.about");
	g_menu_append_section(menu, nullptr, G_MENU_MODEL(app_section));

	auto *const menu_button = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), G_MENU_MODEL(menu));
	gtk_menu_button_set_primary(GTK_MENU_BUTTON(menu_button), TRUE);
	gtk_widget_set_tooltip_text(menu_button, _("Main Menu"));
	adw_header_bar_pack_end(header, menu_button);
	g_object_unref(file_section);
	g_object_unref(machine_section);
	g_object_unref(view_section);
	g_object_unref(app_section);
	g_object_unref(menu);

	// Pages.
	stack_ = GTK_STACK(gtk_stack_new());
	gtk_stack_set_transition_type(stack_, GTK_STACK_TRANSITION_TYPE_CROSSFADE);

	// Welcome page: what the Mac version shows via its Open panel.
	auto *const welcome = ADW_STATUS_PAGE(adw_status_page_new());
	adw_status_page_set_icon_name(welcome, CLK_GTK_APP_ID);
	adw_status_page_set_title(welcome, "Clock Signal");
	adw_status_page_set_description(welcome,
		_("Open a disk, tape, cartridge or snapshot, or drop one here.\n"
		"The emulator will select and configure a machine for you."));
	auto *const welcome_button = gtk_button_new_with_mnemonic(_("_Open…"));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(welcome_button), "win.open");
	gtk_widget_add_css_class(welcome_button, "suggested-action");
	gtk_widget_add_css_class(welcome_button, "pill");
	gtk_widget_set_halign(welcome_button, GTK_ALIGN_CENTER);
	adw_status_page_set_child(welcome, welcome_button);
	gtk_stack_add_named(stack_, GTK_WIDGET(welcome), "welcome");

	// ROM request page: the Mac version's ROM requester sheet.
	roms_page_ = ADW_STATUS_PAGE(adw_status_page_new());
	adw_status_page_set_icon_name(roms_page_, "media-flash-symbolic");
	adw_status_page_set_title(roms_page_, _("ROMs Needed"));
	auto *const roms_button = gtk_button_new_with_mnemonic(_("_Choose ROM Files…"));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(roms_button), "win.choose-roms");
	gtk_widget_add_css_class(roms_button, "suggested-action");
	gtk_widget_add_css_class(roms_button, "pill");
	gtk_widget_set_halign(roms_button, GTK_ALIGN_CENTER);
	adw_status_page_set_child(roms_page_, roms_button);
	gtk_stack_add_named(stack_, GTK_WIDGET(roms_page_), "roms");

	// Machine display.
	gl_area_ = GTK_GL_AREA(gtk_gl_area_new());
	gtk_gl_area_set_allowed_apis(gl_area_, GdkGLAPI(GDK_GL_API_GL | GDK_GL_API_GLES));
	gtk_gl_area_set_auto_render(gl_area_, FALSE);
	gtk_widget_set_focusable(GTK_WIDGET(gl_area_), TRUE);
	gtk_widget_set_hexpand(GTK_WIDGET(gl_area_), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(gl_area_), TRUE);
	g_signal_connect(gl_area_, "realize", G_CALLBACK(+[](GtkWidget *, gpointer self) {
		static_cast<MachineWindow *>(self)->gl_realize();
	}), this);
	g_signal_connect(gl_area_, "unrealize", G_CALLBACK(+[](GtkWidget *, gpointer self) {
		static_cast<MachineWindow *>(self)->gl_unrealize();
	}), this);
	g_signal_connect(gl_area_, "render", G_CALLBACK(+[](GtkGLArea *, GdkGLContext *, gpointer self) -> gboolean {
		return static_cast<MachineWindow *>(self)->gl_render();
	}), this);
	tick_id_ = gtk_widget_add_tick_callback(GTK_WIDGET(gl_area_), [](GtkWidget *widget, GdkFrameClock *, gpointer) -> gboolean {
		gtk_gl_area_queue_render(GTK_GL_AREA(widget));
		return G_SOURCE_CONTINUE;
	}, nullptr, nullptr);
	gtk_stack_add_named(stack_, GTK_WIDGET(gl_area_), "machine");

	toasts_ = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
	adw_toast_overlay_set_child(toasts_, GTK_WIDGET(stack_));

	toolbar_view_ = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
	adw_toolbar_view_add_top_bar(toolbar_view_, GTK_WIDGET(header));
	adw_toolbar_view_set_content(toolbar_view_, GTK_WIDGET(toasts_));
	adw_application_window_set_content(window_, GTK_WIDGET(toolbar_view_));

	// Hide the header bar in fullscreen, so that only the machine's display is visible.
	g_signal_connect(window_, "notify::fullscreened", G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
		auto &self = *static_cast<MachineWindow *>(data);
		adw_toolbar_view_set_reveal_top_bars(self.toolbar_view_, !gtk_window_is_fullscreen(self.window()));
	}), this);

	// Release all keys whenever the window loses focus, as the Mac and Qt versions do.
	g_signal_connect(window_, "notify::is-active", G_CALLBACK(+[](GObject *, GParamSpec *, gpointer data) {
		static_cast<MachineWindow *>(data)->release_all_keys();
	}), this);

	// Keyboard input is captured at the window level, ahead of any widget.
	auto *const keys = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
	g_signal_connect(keys, "key-pressed", G_CALLBACK(+[](GtkEventControllerKey *, guint keyval, guint keycode, GdkModifierType state, gpointer self) -> gboolean {
		return static_cast<MachineWindow *>(self)->key_event(keyval, keycode, state, true);
	}), this);
	g_signal_connect(keys, "key-released", G_CALLBACK(+[](GtkEventControllerKey *, guint keyval, guint keycode, GdkModifierType state, gpointer self) {
		static_cast<MachineWindow *>(self)->key_event(keyval, keycode, state, false);
	}), this);
	gtk_widget_add_controller(GTK_WIDGET(window_), keys);

	// Files can be dropped anywhere on the window.
	auto *const drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
	g_signal_connect(drop, "drop", G_CALLBACK(+[](GtkDropTarget *, const GValue *value, double, double, gpointer self) -> gboolean {
		auto *const list = static_cast<GdkFileList *>(g_value_get_boxed(value));
		GSList *const files = gdk_file_list_get_files(list);
		const bool result = static_cast<MachineWindow *>(self)->handle_drop(files);
		g_slist_free(files);
		return result;
	}), this);
	gtk_widget_add_controller(GTK_WIDGET(window_), GTK_EVENT_CONTROLLER(drop));

	// Shut the machine down before the window goes; the object goes with the window.
	g_signal_connect(window_, "close-request", G_CALLBACK(+[](GtkWindow *, gpointer self) -> gboolean {
		static_cast<MachineWindow *>(self)->delete_machine();
		return FALSE;
	}), this);
	g_signal_connect(window_, "destroy", G_CALLBACK(+[](GtkWidget *, gpointer self) {
		delete static_cast<MachineWindow *>(self);
	}), this);

	set_phase(Phase::Welcome);
}

MachineWindow::~MachineWindow() {
	delete_machine();
}

// MARK: - Phases and window chrome.

void MachineWindow::set_phase(const Phase phase) {
	phase_ = phase;
	switch(phase) {
		case Phase::Welcome:		gtk_stack_set_visible_child_name(stack_, "welcome");	break;
		case Phase::RequestingROMs:	gtk_stack_set_visible_child_name(stack_, "roms");		break;
		case Phase::Running:
			gtk_stack_set_visible_child_name(stack_, "machine");
			gtk_widget_grab_focus(GTK_WIDGET(gl_area_));
		break;
	}
	update_title();
	update_actions();
}

void MachineWindow::update_title() {
	std::string title = "Clock Signal";
	std::string subtitle;

	switch(phase_) {
		case Phase::Welcome: break;
		case Phase::RequestingROMs:
			title = display_name_;
			subtitle = _("Provide ROMs");
		break;
		case Phase::Running:
			title = display_name_;
			if(!targets_.empty()) {
				subtitle = Machine::LongNameForTargetMachine(targets_.front()->machine);
				if(subtitle == title) subtitle.clear();
			}
		break;
	}

	adw_window_title_set_title(title_, title.c_str());
	adw_window_title_set_subtitle(title_, subtitle.c_str());
	gtk_window_set_title(window(), title.c_str());
}

void MachineWindow::update_actions() {
	const auto enable = [this](const char *name, const bool enabled) {
		auto *const action = g_action_map_lookup_action(G_ACTION_MAP(window_), name);
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
	};

	const bool running = machine_ && phase_ == Phase::Running;
	enable("insert", running && machine_->media_target());
	enable("soft-reset", running && machine_->soft_resettable());
	enable("hard-reset", running && machine_->hard_resettable());
	enable("choose-roms", phase_ == Phase::RequestingROMs);

	// Offer keyboard-as-joystick only where both exist and the keyboard would otherwise take every key.
	bool joystick_choice = false;
	if(running) {
		auto *const keyboard = machine_->keyboard_machine();
		auto *const joystick = machine_->joystick_machine();
		joystick_choice = keyboard && joystick && keyboard->keyboard().is_exclusive();
	}
	enable("keyboard-as-joystick", joystick_choice);
}

void MachineWindow::show_toast(const std::string &text) {
	adw_toast_overlay_add_toast(toasts_, adw_toast_new(text.c_str()));
}

// MARK: - Machine lifecycle.

void MachineWindow::open(GFile *const file) {
	const auto path = path_of(file);
	if(path.empty()) {
		show_toast(_("Only local files can be opened."));
		return;
	}

	// As per the Mac version's document model: an idle window takes the file; a busy one spawns a sibling.
	if(machine_) {
		auto *const sibling = create(gtk_window_get_application(window()));
		sibling->launch_file(path);
		return;
	}
	launch_file(path);
}

bool MachineWindow::launch_file(const std::filesystem::path &path) {
	auto targets = Analyser::Static::GetTargets(path);
	if(targets.empty()) {
		char *const text = g_strdup_printf(_("Clock Signal doesn't recognise “%s”."), path.filename().c_str());
		show_toast(text);
		g_free(text);
		return false;
	}

	targets_ = std::move(targets);
	source_file_ = path;
	display_name_ = path.filename().string();
	launch_targets();
	return true;
}

void MachineWindow::launch_targets() {
	delete_machine();

	Machine::Error error;
	auto machine = Machine::MachineForTargets(targets_, ROMs::fetcher(missing_roms_), error);

	if(error != Machine::Error::None || !machine) {
		if(error == Machine::Error::MissingROM) {
			const std::string description =
				std::string(_("To run this machine, Clock Signal needs the following ROM images. "
				"Drop them here or choose them from your files; they'll be copied to ")) +
				ROMs::display_path(ROMs::user_path()) + ".\n\n" + ROMs::to_utf8(missing_roms_.description(0, L'•'));
			adw_status_page_set_description(roms_page_, description.c_str());
			set_phase(Phase::RequestingROMs);
		} else {
			show_toast(_("This machine couldn't be started."));
			set_phase(Phase::Welcome);
		}
		return;
	}

	machine_ = std::move(machine);
	const auto machine_type = targets_.front()->machine;

	// User-friendly default options, as per the Qt version; per-machine stored choices come later.
	if(auto *const configurable = machine_->configurable_device()) {
		auto options = Machine::AllOptionsByMachineName();
		auto found = options.find(Machine::LongNameForTargetMachine(machine_type));
		if(found != options.end() && found->second) {
			configurable->set_options(found->second);
		}
	}

	// The scan target is created on the GL thread at the next render.
	pending_producer_ = machine_->scan_producer();

	if(auto *const audio_producer = machine_->audio_producer()) {
		if(auto *const speaker = audio_producer->get_speaker()) {
			if(!audio_.start(*speaker)) {
				show_toast(_("No audio output is available."));
			}
		}
	}

	auto *const keyboard = machine_->keyboard_machine();
	keyboard_as_joystick_ = !keyboard;
	g_action_change_state(
		g_action_map_lookup_action(G_ACTION_MAP(window_), "keyboard-as-joystick"),
		g_variant_new_boolean(keyboard_as_joystick_)
	);

	if(auto *const timed = machine_->timed_machine()) {
		runner_.start(timed, &machine_mutex_);
	}

	set_phase(Phase::Running);
	gtk_gl_area_queue_render(gl_area_);
}

void MachineWindow::delete_machine() {
	runner_.stop();
	audio_.stop();
	pressed_keycodes_.clear();

	if(scan_target_ && gtk_widget_get_realized(GTK_WIDGET(gl_area_))) {
		gtk_gl_area_make_current(gl_area_);
		scan_target_.reset();
	}
	scan_target_.reset();
	pending_producer_ = nullptr;
	machine_.reset();
}

bool MachineWindow::insert_file(const std::filesystem::path &path) {
	if(!machine_) return false;
	auto *const media_target = machine_->media_target();
	if(!media_target) return false;

	const auto media = Analyser::Static::GetMedia(path);
	if(media.empty()) return false;

	std::lock_guard lock(machine_mutex_);
	return media_target->insert_media(media);
}

// MARK: - Files.

void MachineWindow::choose_file(const bool insert) {
	auto *const dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, insert ? _("Insert") : _("Open"));

	struct Request { MachineWindow *self; bool insert; };
	gtk_file_dialog_open(dialog, window(), nullptr, [](GObject *source, GAsyncResult *result, gpointer data) {
		const auto request = *static_cast<Request *>(data);
		delete static_cast<Request *>(data);

		GFile *const file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, nullptr);
		if(!file) return;

		if(request.insert) {
			if(!request.self->insert_file(path_of(file))) {
				request.self->show_toast(_("That file can't be inserted into this machine."));
			}
		} else {
			request.self->open(file);
		}
		g_object_unref(file);
	}, new Request{this, insert});
	g_object_unref(dialog);
}

void MachineWindow::choose_roms() {
	auto *const dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, _("Choose ROM Files"));
	gtk_file_dialog_open_multiple(dialog, window(), nullptr, [](GObject *source, GAsyncResult *result, gpointer data) {
		GListModel *const files = gtk_file_dialog_open_multiple_finish(GTK_FILE_DIALOG(source), result, nullptr);
		if(!files) return;

		std::vector<std::filesystem::path> paths;
		for(guint c = 0; c < g_list_model_get_n_items(files); ++c) {
			auto *const file = G_FILE(g_list_model_get_item(files, c));
			paths.push_back(path_of(file));
			g_object_unref(file);
		}
		g_object_unref(files);
		static_cast<MachineWindow *>(data)->install_roms(paths);
	}, this);
	g_object_unref(dialog);
}

bool MachineWindow::handle_drop(const GSList *const files) {
	std::vector<std::filesystem::path> paths;
	for(const GSList *item = files; item; item = item->next) {
		const auto path = path_of(G_FILE(item->data));
		if(!path.empty()) paths.push_back(path);
	}
	if(paths.empty()) return false;

	switch(phase_) {
		case Phase::Welcome:
			return launch_file(paths.front());

		case Phase::RequestingROMs:
			install_roms(paths);
			return true;

		case Phase::Running:
			// As per the Mac version: insert if possible, otherwise start afresh with the new file.
			if(insert_file(paths.front())) return true;
			return launch_file(paths.front());
	}
	return false;
}

void MachineWindow::install_roms(const std::vector<std::filesystem::path> &files) {
	bool any_installed = false;
	std::string unrecognised;
	for(const auto &file: files) {
		const auto result = ROMs::install(file);
		if(result.installed) {
			any_installed = true;
		} else {
			if(!unrecognised.empty()) unrecognised += ", ";
			unrecognised += file.filename().string();
		}
	}

	if(!unrecognised.empty()) {
		char *const text = g_strdup_printf(_("Not a ROM that Clock Signal knows: %s"), unrecognised.c_str());
		show_toast(text);
		g_free(text);
	}
	if(any_installed) launch_targets();
}

// MARK: - Display.

void MachineWindow::gl_realize() {
	gtk_gl_area_make_current(gl_area_);
	if(gtk_gl_area_get_error(gl_area_)) return;

	api_ = gtk_gl_area_get_api(gl_area_) == GDK_GL_API_GLES ?
		Outputs::Display::OpenGL::API::OpenGLES3 : Outputs::Display::OpenGL::API::OpenGL32Core;
}

void MachineWindow::gl_unrealize() {
	// Without a context there is nowhere to draw to, so the machine can't continue.
	if(scan_target_) {
		gtk_gl_area_make_current(gl_area_);
		runner_.stop();
		scan_target_.reset();
		if(machine_) pending_producer_ = machine_->scan_producer();
	}
}

bool MachineWindow::gl_render() {
	if(gtk_gl_area_get_error(gl_area_)) return false;

	const int scale = gtk_widget_get_scale_factor(GTK_WIDGET(gl_area_));
	const int width = gtk_widget_get_width(GTK_WIDGET(gl_area_)) * scale;
	const int height = gtk_widget_get_height(GTK_WIDGET(gl_area_)) * scale;

	GLint framebuffer = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);

	if(pending_producer_) {
		scan_target_ = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(api_, GLuint(framebuffer));
		{
			std::lock_guard lock(machine_mutex_);
			pending_producer_->set_scan_target(scan_target_.get());
		}
		pending_producer_ = nullptr;
		framebuffer_ = GLuint(framebuffer);
		if(machine_ && !runner_.paused()) {
			// Restart after an unrealize, if the machine had to stop.
			if(auto *const timed = machine_->timed_machine()) {
				runner_.start(timed, &machine_mutex_);
			}
		}
	}

	glViewport(0, 0, width, height);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if(scan_target_) {
		// GTK may hand over a different framebuffer after a resize.
		if(GLuint(framebuffer) != framebuffer_) {
			framebuffer_ = GLuint(framebuffer);
			scan_target_->set_target_framebuffer(framebuffer_);
		}

		// The scan target leaves its own program bound; GTK doesn't expect that.
		Outputs::Display::OpenGL::Shader::unbind();
		scan_target_->update(width, height);
		scan_target_->draw(width, height);
		Outputs::Display::OpenGL::Shader::unbind();
	}
	return true;
}

// MARK: - Input.

void MachineWindow::release_all_keys() {
	pressed_keycodes_.clear();
	if(!machine_) return;

	std::lock_guard lock(machine_mutex_);
	if(auto *const keyboard = machine_->keyboard_machine()) {
		keyboard->clear_all_keys();
	}
	if(auto *const joystick = machine_->joystick_machine()) {
		for(const auto &stick: joystick->get_joysticks()) {
			stick->reset_all_inputs();
		}
	}
}

bool MachineWindow::key_event(const unsigned keyval, const unsigned keycode, const GdkModifierType state, const bool pressed) {
	if(!machine_ || phase_ != Phase::Running) return false;

	// Leave the window's own shortcuts alone: Ctrl+Shift+anything, and F11 for fullscreen.
	// Everything else, Ctrl included, belongs to the emulated machine.
	if(pressed) {
		const auto modifiers = state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK);
		if(modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) && keyval != GDK_KEY_Shift_L && keyval != GDK_KEY_Shift_R
			&& keyval != GDK_KEY_Control_L && keyval != GDK_KEY_Control_R) {
			// The modifiers have already reached the machine; lift them so they don't stick.
			release_all_keys();
			return false;
		}
		if(keyval == GDK_KEY_F11) {
			release_all_keys();
			return false;
		}
	}

	const auto key = key_for_keycode(keycode);
	if(!key) return false;

	bool is_repeat = false;
	if(pressed) {
		is_repeat = !pressed_keycodes_.insert(keycode).second;
	} else {
		if(!pressed_keycodes_.erase(keycode)) return false;	// Not pressed while the machine was listening.
	}

	const gunichar character = gdk_keyval_to_unicode(keyval);
	const char value = (character > 0 && character < 128) ? char(character) : '\0';

	std::lock_guard lock(machine_mutex_);

	if(!keyboard_as_joystick_) {
		if(auto *const keyboard_machine = machine_->keyboard_machine()) {
			auto &keyboard = keyboard_machine->keyboard();
			keyboard.set_key_pressed(*key, value, pressed, is_repeat);
			if(keyboard.is_exclusive() || keyboard.observed_keys().count(*key)) {
				return true;
			}
		}
	}

	auto *const joystick_machine = machine_->joystick_machine();
	if(!joystick_machine) return true;
	const auto &joysticks = joystick_machine->get_joysticks();
	if(joysticks.empty()) return true;

	using Key = Inputs::Keyboard::Key;
	using Input = Inputs::Joystick::Input;
	auto &joystick = *joysticks[0];
	switch(*key) {
		case Key::Left:		joystick.set_input(Input::Left, pressed);	break;
		case Key::Right:	joystick.set_input(Input::Right, pressed);	break;
		case Key::Up:		joystick.set_input(Input::Up, pressed);		break;
		case Key::Down:		joystick.set_input(Input::Down, pressed);	break;
		case Key::Space:	joystick.set_input(Input::Fire, pressed);	break;
		case Key::A:		joystick.set_input(Input(Input::Fire, 0), pressed);	break;
		case Key::S:		joystick.set_input(Input(Input::Fire, 1), pressed);	break;
		case Key::D:		joystick.set_input(Input(Input::Fire, 2), pressed);	break;
		case Key::F:		joystick.set_input(Input(Input::Fire, 3), pressed);	break;
		default:
			if(value) joystick.set_input(Input(value), pressed);
			else joystick.set_input(Input::Fire, pressed);
		break;
	}
	return true;
}
