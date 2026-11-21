//
//  main.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include <adwaita.h>

#include "i18n.hpp"
#include "MachineWindow.hpp"

#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace GTKFrontend;

namespace {

void show_about(GtkApplication *const application) {
	auto *const about = ADW_ABOUT_DIALOG(adw_about_dialog_new());
	adw_about_dialog_set_application_name(about, "Clock Signal");
	adw_about_dialog_set_application_icon(about, CLK_GTK_APP_ID);
	adw_about_dialog_set_developer_name(about, "Thomas Harte");
	adw_about_dialog_set_website(about, "https://github.com/TomHarte/CLK");
	adw_about_dialog_set_issue_url(about, "https://github.com/goliv04053/CLK/issues");
	adw_about_dialog_set_license_type(about, GTK_LICENSE_MIT_X11);
	adw_about_dialog_set_copyright(about, "© 2015–2026 Thomas Harte");
	adw_about_dialog_set_comments(about,
		_("Clock Signal is a latency-hating emulator of 8- and 16-bit platforms, "
		"by Thomas Harte, with a heavy signal processing tilt for accurate reproduction of original outputs.\n\n"
		"This is an unofficial GTK4/libadwaita frontend for Linux, based on the Mac version's features "
		"and on the earlier Qt and SDL frontends."));

	const char *developers[] = {"Thomas Harte https://github.com/TomHarte", nullptr};
	adw_about_dialog_set_developers(about, developers);
	const char *port[] = {
		"Claude (Anthropic)",
		nullptr
	};
	adw_about_dialog_add_credit_section(about, _("GTK Frontend"), port);
	const char *requested[] = {"Gabriel https://github.com/goliv04053", nullptr};
	adw_about_dialog_add_credit_section(about, _("Requested and Tested by"), requested);

	GtkWindow *const parent = gtk_application_get_active_window(application);
	adw_dialog_present(ADW_DIALOG(about), parent ? GTK_WIDGET(parent) : nullptr);
}

void setup_localisation() {
	setlocale(LC_ALL, "");

	// Inside an AppImage, translations live in the image rather than the system.
	std::string locale_dir = CLK_GTK_LOCALEDIR;
	if(const char *const appdir = std::getenv("APPDIR")) {
		locale_dir = std::string(appdir) + "/usr/share/locale";
	}
	bindtextdomain(GETTEXT_PACKAGE, locale_dir.c_str());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
}

}

int main(int argc, char *argv[]) {
	setup_localisation();

	AdwApplication *const application = adw_application_new(CLK_GTK_APP_ID, G_APPLICATION_HANDLES_OPEN);
	g_set_application_name("Clock Signal");

	static const GActionEntry entries[] = {
		{"new-window", [](GSimpleAction *, GVariant *, gpointer application) {
			MachineWindow::create(GTK_APPLICATION(application));
		}, nullptr, nullptr, nullptr, {}},
		{"about", [](GSimpleAction *, GVariant *, gpointer application) {
			show_about(GTK_APPLICATION(application));
		}, nullptr, nullptr, nullptr, {}},
		{"quit", [](GSimpleAction *, GVariant *, gpointer application) {
			// Close each window properly, so that every machine is shut down in order.
			GList *const windows = gtk_application_get_windows(GTK_APPLICATION(application));
			for(GList *window = windows; window; window = window->next) {
				gtk_window_close(GTK_WINDOW(window->data));
			}
		}, nullptr, nullptr, nullptr, {}},
	};
	g_action_map_add_action_entries(G_ACTION_MAP(application), entries, G_N_ELEMENTS(entries), application);

	// Shortcuts. While a machine is running Ctrl alone belongs to the machine, so each
	// shortcut also has a Ctrl+Shift form that always works.
	struct Accel { const char *action; const char *accels[3]; };
	static const Accel accels[] = {
		{"app.new-window",		{"<Control>n", "<Control><Shift>n", nullptr}},
		{"app.quit",			{"<Control>q", "<Control><Shift>q", nullptr}},
		{"win.open",			{"<Control>o", "<Control><Shift>o", nullptr}},
		{"win.insert",			{"<Control>i", "<Control><Shift>i", nullptr}},
		{"win.soft-reset",		{"<Control><Shift>r", nullptr, nullptr}},
		{"win.hard-reset",		{"<Control><Shift>h", nullptr, nullptr}},
		{"win.fullscreen",		{"F11", "<Control><Shift>f", nullptr}},
		{"window.close",		{"<Control>w", "<Control><Shift>w", nullptr}},
	};
	for(const auto &accel: accels) {
		gtk_application_set_accels_for_action(GTK_APPLICATION(application), accel.action, accel.accels);
	}

	g_signal_connect(application, "activate", G_CALLBACK(+[](GApplication *application, gpointer) {
		MachineWindow::create(GTK_APPLICATION(application));
	}), nullptr);

	g_signal_connect(application, "open", G_CALLBACK(+[](GApplication *application, GFile **files, gint count, const gchar *, gpointer) {
		for(gint c = 0; c < count; ++c) {
			// Reuse an idle window if there is one; otherwise each file gets its own window.
			MachineWindow *target = nullptr;
			for(GList *window = gtk_application_get_windows(GTK_APPLICATION(application)); window; window = window->next) {
				auto *const candidate = MachineWindow::from(GTK_WINDOW(window->data));
				if(candidate && !candidate->has_machine()) {
					target = candidate;
					break;
				}
			}
			if(target) {
				gtk_window_present(target->window());
				target->open(files[c]);
			} else {
				MachineWindow::create(GTK_APPLICATION(application), files[c]);
			}
		}
	}), nullptr);

	const int status = g_application_run(G_APPLICATION(application), argc, argv);
	g_object_unref(application);
	return status;
}
