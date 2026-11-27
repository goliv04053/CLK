//
//  MachinePicker.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include <gtk/gtk.h>

#include "../../Analyser/Static/StaticAnalyser.hpp"

#include <functional>
#include <string>

namespace GTKFrontend::MachinePicker {

/// Receives the machine that the user configured, and a name for it.
using Handler = std::function<void(Analyser::Static::TargetList &&, const std::string &name)>;

/// Presents the equivalent of the Mac version's machine picker: a list of machines,
/// each with its options. The choices made are remembered for next time.
/// @c handler is called only if the user starts a machine.
void present(GtkWidget *parent, Handler handler);

}
