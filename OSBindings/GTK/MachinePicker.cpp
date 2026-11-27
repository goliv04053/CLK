//
//  MachinePicker.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "MachinePicker.hpp"

#include "i18n.hpp"
#include "Settings.hpp"

#include <adwaita.h>

#include "../../Analyser/Static/Acorn/Target.hpp"
#include "../../Analyser/Static/Amiga/Target.hpp"
#include "../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../Analyser/Static/AppleII/Target.hpp"
#include "../../Analyser/Static/AtariST/Target.hpp"
#include "../../Analyser/Static/Commodore/Target.hpp"
#include "../../Analyser/Static/Enterprise/Target.hpp"
#include "../../Analyser/Static/Macintosh/Target.hpp"
#include "../../Analyser/Static/MSX/Target.hpp"
#include "../../Analyser/Static/Oric/Target.hpp"
#include "../../Analyser/Static/PCCompatible/Target.hpp"
#include "../../Analyser/Static/TandyCoCo/Target.hpp"
#include "../../Analyser/Static/Thomson/Target.hpp"
#include "../../Analyser/Static/ZX8081/Target.hpp"
#include "../../Analyser/Static/ZXSpectrum/Target.hpp"
#include "../../Storage/FileBundle/FileBundle.hpp"

#include <map>
#include <memory>
#include <vector>

namespace GTKFrontend::MachinePicker {
namespace {

// MARK: - The catalogue.
//
// Machines and options follow the Mac version's picker (MachinePicker.xib and the
// CSStaticAnalyser initialisers it calls), in the same order and with the same choices.
// The Apple IIgs is omitted, as it is from the Mac version's release builds.

struct Choice {
	const char *label;		// Marked for translation with N_() where it isn't a proper name.
	int value;
};

struct Option {
	enum class Kind {
		Choice,		// A pop-up list of choices.
		Toggle,		// On or off.
		Folder,		// An optional local folder.
	} kind;
	const char *key;
	const char *title;
	std::vector<Choice> choices = {};
	int default_value = 0;
};

struct Values {
	std::map<std::string, int> numbers;
	std::map<std::string, std::string> strings;

	int operator[](const std::string &key) const {
		const auto value = numbers.find(key);
		return value == numbers.end() ? 0 : value->second;
	}
	bool flag(const std::string &key) const {
		return (*this)[key] != 0;
	}
};

struct Machine {
	const char *id;
	const char *name;
	std::vector<Option> options;
	std::function<std::unique_ptr<Analyser::Static::Target>(const Values &)> make;
	const char *note = nullptr;
};

using Kind = Option::Kind;

const std::vector<Machine> &catalogue() {
	static const std::vector<Machine> machines = {
		{
			"amiga", "Amiga",
			{
				{Kind::Choice, "chip-ram", N_("Chip Memory"), {{"512 KB", 512}, {"1 MB", 1024}, {"2 MB", 2048}}, 512},
				{Kind::Choice, "fast-ram", N_("Fast Memory"),
					{{N_("None"), 0}, {"1 MB", 1024}, {"2 MB", 2048}, {"4 MB", 4096}, {"8 MB", 8192}}, 0},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Amiga::Target;
				auto target = std::make_unique<Target>();
				switch(values["chip-ram"]) {
					default:	target->chip_ram = Target::ChipRAM::FiveHundredAndTwelveKilobytes;	break;
					case 1024:	target->chip_ram = Target::ChipRAM::OneMegabyte;					break;
					case 2048:	target->chip_ram = Target::ChipRAM::TwoMegabytes;					break;
				}
				switch(values["fast-ram"]) {
					default:	target->fast_ram = Target::FastRAM::None;			break;
					case 1024:	target->fast_ram = Target::FastRAM::OneMegabyte;	break;
					case 2048:	target->fast_ram = Target::FastRAM::TwoMegabytes;	break;
					case 4096:	target->fast_ram = Target::FastRAM::FourMegabytes;	break;
					case 8192:	target->fast_ram = Target::FastRAM::EightMegabytes;	break;
				}
				return target;
			}
		},
		{
			"cpc", "Amstrad CPC",
			{
				{Kind::Choice, "model", N_("Model"), {{"CPC464", 464}, {"CPC664", 664}, {"CPC6128", 6128}}, 6128},
			},
			[](const Values &values) {
				using Target = Analyser::Static::AmstradCPC::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					case 464:	target->model = Target::Model::CPC464;	break;
					case 664:	target->model = Target::Model::CPC664;	break;
					default:	target->model = Target::Model::CPC6128;	break;
				}
				return target;
			}
		},
		{
			"appleii", "Apple II",
			{
				{Kind::Choice, "model", N_("Model"),
					{{"Apple II", 0}, {"Apple II+", 1}, {"Apple IIe", 2}, {N_("Enhanced IIe"), 3}}, 0},
				{Kind::Choice, "disk-controller", N_("Disk Controller"),
					{{N_("Sixteen Sector"), 16}, {N_("Thirteen Sector"), 13}, {N_("None"), 0}}, 16},
				{Kind::Toggle, "mockingboard", N_("Mockingboard"), {}, 0},
			},
			[](const Values &values) {
				using Target = Analyser::Static::AppleII::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					default:	target->model = Target::Model::II;			break;
					case 1:		target->model = Target::Model::IIplus;		break;
					case 2:		target->model = Target::Model::IIe;			break;
					case 3:		target->model = Target::Model::EnhancedIIe;	break;
				}
				switch(values["disk-controller"]) {
					default:	target->disk_controller = Target::DiskController::None;				break;
					case 16:	target->disk_controller = Target::DiskController::SixteenSector;	break;
					case 13:	target->disk_controller = Target::DiskController::ThirteenSector;	break;
				}
				target->has_mockingboard = values.flag("mockingboard");
				return target;
			}
		},
		{
			"archimedes", "Archimedes",
			{},
			[](const Values &) {
				return std::make_unique<Analyser::Static::Acorn::ArchimedesTarget>();
			},
			N_("At present only a 4 MB 26-bit Archimedes is available.")
		},
		{
			"atarist", "Atari ST",
			{
				{Kind::Choice, "memory", N_("Memory"), {{"512 KB", 512}, {"1 MB", 1024}, {"4 MB", 4096}}, 512},
			},
			[](const Values &values) {
				using Target = Analyser::Static::AtariST::Target;
				auto target = std::make_unique<Target>();
				switch(values["memory"]) {
					default:	target->memory_size = Target::MemorySize::FiveHundredAndTwelveKilobytes;	break;
					case 1024:	target->memory_size = Target::MemorySize::OneMegabyte;						break;
					case 4096:	target->memory_size = Target::MemorySize::FourMegabytes;					break;
				}
				return target;
			}
		},
		{
			"bbcmicro", "BBC Micro",
			{
				{Kind::Toggle, "dfs", N_("Disk Filing System"), {}, 1},
				{Kind::Toggle, "adfs", N_("Advanced Disk Filing System"), {}, 1},
				{Kind::Toggle, "beebsid", "BeebSID", {}, 0},
				{Kind::Choice, "second-processor", N_("Second Processor"),
					{{N_("None"), 0}, {"65C02", 6502}, {"Z80", 80}}, 0},
				{Kind::Toggle, "sideways-ram", N_("Fill Unused ROM Banks With Sideways RAM"), {}, 1},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Acorn::BBCMicroTarget;
				auto target = std::make_unique<Target>();
				target->has_1770dfs = values.flag("dfs");
				target->has_adfs = values.flag("adfs");
				target->has_beebsid = values.flag("beebsid");
				target->has_sideways_ram = values.flag("sideways-ram");
				switch(values["second-processor"]) {
					default:	target->tube_processor = Target::TubeProcessor::None;		break;
					case 6502:	target->tube_processor = Target::TubeProcessor::WDC65C02;	break;
					case 80:	target->tube_processor = Target::TubeProcessor::Z80;		break;
				}
				return target;
			}
		},
		{
			"c16plus4", "Commodore TED",
			{
				{Kind::Toggle, "c1541", N_("Attach C-1541 Disk Drive"), {}, 1},
			},
			[](const Values &values) {
				auto target = std::make_unique<Analyser::Static::Commodore::Plus4Target>();
				target->has_c1541 = values.flag("c1541");
				return target;
			}
		},
		{
			"electron", "Electron",
			{
				{Kind::Toggle, "dfs", N_("Disk Filing System"), {}, 1},
				{Kind::Toggle, "adfs", N_("Advanced Disk Filing System"), {}, 1},
				{Kind::Toggle, "ap6", N_("Advanced Plus 6 Utility ROM"), {}, 0},
				{Kind::Toggle, "sideways-ram", N_("Fill Unused ROM Banks With Sideways RAM"), {}, 1},
			},
			[](const Values &values) {
				auto target = std::make_unique<Analyser::Static::Acorn::ElectronTarget>();
				target->has_dfs = values.flag("dfs");
				target->has_pres_adfs = values.flag("adfs");
				target->has_ap6_rom = values.flag("ap6");
				target->has_sideways_ram = values.flag("sideways-ram");
				return target;
			}
		},
		{
			"enterprise", "Enterprise",
			{
				{Kind::Choice, "model", N_("Machine"),
					{{"Enterprise 256", 256}, {"Enterprise 128", 128}, {"Enterprise 64", 64}}, 128},
				{Kind::Choice, "speed", N_("Speed"), {{"4 MHz", 4}, {"6 MHz", 6}}, 4},
				{Kind::Choice, "exos", "EXOS",
					{{N_("Version 2.1"), 21}, {N_("Version 2.0"), 20}, {N_("Version 1.0"), 10}}, 21},
				{Kind::Choice, "basic", "BASIC",
					{{N_("Version 2.1"), 21}, {N_("Version 1.1"), 11}, {N_("Version 1.0"), 10}, {N_("None"), 0}}, 21},
				{Kind::Choice, "dos", "DOS", {{"EXDOS", 1}, {N_("None"), 0}}, 0},
				{Kind::Folder, "exposed-path", N_("Local Folder Exposed to the Machine")},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Enterprise::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					case 64:	target->model = Target::Model::Enterprise64;	break;
					default:	target->model = Target::Model::Enterprise128;	break;
					case 256:	target->model = Target::Model::Enterprise256;	break;
				}
				target->speed = values["speed"] == 6 ? Target::Speed::SixMHz : Target::Speed::FourMHz;
				switch(values["exos"]) {
					default:	target->exos_version = Target::EXOSVersion::v21;	break;
					case 20:	target->exos_version = Target::EXOSVersion::v20;	break;
					case 10:	target->exos_version = Target::EXOSVersion::v10;	break;
				}
				switch(values["basic"]) {
					case 0:		target->basic_version = Target::BASICVersion::None;	break;
					default:	target->basic_version = Target::BASICVersion::v21;	break;
					case 11:	target->basic_version = Target::BASICVersion::v11;	break;
					case 10:	target->basic_version = Target::BASICVersion::v10;	break;
				}
				target->dos = values["dos"] == 1 ? Target::DOS::EXDOS : Target::DOS::None;

				const auto path = values.strings.find("exposed-path");
				if(path != values.strings.end() && !path->second.empty()) {
					target->media.file_bundles.push_back(
						std::make_shared<Storage::FileBundle::LocalFSFileBundle>(path->second));
				}
				return target;
			}
		},
		{
			"mac", "Macintosh",
			{
				{Kind::Choice, "model", N_("Model"), {{"128k", 0}, {"512k", 1}, {"512ke", 2}, {"Plus", 3}}, 3},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Macintosh::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					case 0:		target->model = Target::Model::Mac128k;		break;
					case 1:		target->model = Target::Model::Mac512k;		break;
					case 2:		target->model = Target::Model::Mac512ke;	break;
					default:	target->model = Target::Model::MacPlus;		break;
				}
				return target;
			}
		},
		{
			"msx", "MSX",
			{
				{Kind::Choice, "model", N_("Model"), {{"MSX 1", 1}, {"MSX 2", 2}}, 1},
				{Kind::Choice, "region", N_("Region"),
					{{N_("European (PAL)"), 0}, {N_("American (NTSC)"), 1}, {N_("Japanese (NTSC)"), 2}}, 0},
				{Kind::Toggle, "disk-drive", N_("Attach Disk Drive"), {}, 1},
				{Kind::Toggle, "msx-music", N_("Attach MSX-MUSIC"), {}, 0},
			},
			[](const Values &values) {
				using Target = Analyser::Static::MSX::Target;
				auto target = std::make_unique<Target>();
				target->model = values["model"] == 2 ? Target::Model::MSX2 : Target::Model::MSX1;
				switch(values["region"]) {
					default:	target->region = Target::Region::Europe;	break;
					case 1:		target->region = Target::Region::USA;		break;
					case 2:		target->region = Target::Region::Japan;		break;
				}
				target->has_disk_drive = values.flag("disk-drive");
				target->has_msx_music = values.flag("msx-music");
				return target;
			}
		},
		{
			"oric", "Oric",
			{
				{Kind::Choice, "model", N_("Model"), {{"Oric-1", 0}, {"Oric Atmos", 1}, {"Pravetz 8D", 2}}, 1},
				{Kind::Choice, "disk-interface", N_("Disk Interface"),
					{{N_("None"), 0}, {"Microdisc", 1}, {"Jasmin", 3}, {"8DOS", 2}, {"Byte Drive 500", 4}}, 0},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Oric::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					default:	target->rom = Target::ROM::BASIC10;	break;
					case 1:		target->rom = Target::ROM::BASIC11;	break;
					case 2:		target->rom = Target::ROM::Pravetz;	break;
				}
				switch(values["disk-interface"]) {
					default:	target->disk_interface = Target::DiskInterface::None;		break;
					case 1:		target->disk_interface = Target::DiskInterface::Microdisc;	break;
					case 2:		target->disk_interface = Target::DiskInterface::Pravetz;	break;
					case 3:		target->disk_interface = Target::DiskInterface::Jasmin;		break;
					case 4:		target->disk_interface = Target::DiskInterface::BD500;		break;
				}
				return target;
			}
		},
		{
			"pc", N_("PC Compatible"),
			{
				{Kind::Choice, "video-adaptor", N_("Video Adaptor"), {{"MDA", 0}, {"CGA", 1}}, 1},
				{Kind::Choice, "speed", N_("Speed"), {{N_("Similar to Original"), 8086}, {N_("Turbo"), 80286}}, 8086},
			},
			[](const Values &values) {
				using Target = Analyser::Static::PCCompatible::Target;
				auto target = std::make_unique<Target>();
				target->adaptor = values["video-adaptor"] == 1 ? Target::VideoAdaptor::CGA : Target::VideoAdaptor::MDA;
				target->model = values["speed"] == 80286 ?
					Analyser::Static::PCCompatible::Model::TurboXT : Analyser::Static::PCCompatible::Model::XT;
				return target;
			}
		},
		{
			"tandycoco", "Tandy CoCo",
			{
				{Kind::Choice, "memory", N_("Memory Size"), {{"32 KB", 32}, {"64 KB", 64}}, 64},
				{Kind::Toggle, "disk-drive", N_("Disk Drive"), {}, 1},
			},
			[](const Values &values) {
				using Target = Analyser::Static::TandyCoCo::Target;
				auto target = std::make_unique<Target>();
				target->memory_size = values["memory"] == 32 ?
					Target::MemorySize::ThirtyTwoKB : Target::MemorySize::SixtyFourKB;
				target->has_disk_drive = values.flag("disk-drive");
				return target;
			}
		},
		{
			"thomsonmo", "Thomson MO",
			{
				{Kind::Choice, "model", N_("Machine"), {{"MO5", 5}, {"MO6", 6}, {"Olivetti Prodest 128", 128}}, 6},
				{Kind::Toggle, "disk-drive", N_("Disk Drive"), {}, 1},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Thomson::MOTarget;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					case 5:		target->model = Target::Model::MO5v11;		break;
					default:	target->model = Target::Model::MO6v3;		break;
					case 128:	target->model = Target::Model::Prodest128;	break;
				}
				target->floppy = values.flag("disk-drive") ? Target::Floppy::CD90_640 : Target::Floppy::None;
				return target;
			}
		},
		{
			"vic20", "Vic-20",
			{
				{Kind::Choice, "region", N_("Region"),
					{
						{N_("European (PAL)"), 0}, {N_("American (NTSC)"), 1}, {N_("Danish (PAL)"), 2},
						{N_("Swedish (PAL)"), 3}, {N_("Japanese (NTSC)"), 4}
					}, 0},
				{Kind::Choice, "memory", N_("Memory Size"), {{N_("Unexpanded"), 0}, {"8 KB", 8}, {"32 KB", 32}}, 0},
				{Kind::Toggle, "c1540", N_("Attach C-1540 Disk Drive"), {}, 1},
			},
			[](const Values &values) {
				using Target = Analyser::Static::Commodore::Vic20Target;
				auto target = std::make_unique<Target>();
				switch(values["region"]) {
					default:	target->region = Target::Region::European;	break;
					case 1:		target->region = Target::Region::American;	break;
					case 2:		target->region = Target::Region::Danish;	break;
					case 3:		target->region = Target::Region::Swedish;	break;
					case 4:		target->region = Target::Region::Japanese;	break;
				}
				switch(values["memory"]) {
					default:	target->set_memory_model(Target::MemoryModel::Unexpanded);	break;
					case 8:		target->set_memory_model(Target::MemoryModel::EightKB);		break;
					case 32:	target->set_memory_model(Target::MemoryModel::ThirtyTwoKB);	break;
				}
				target->has_c1540 = values.flag("c1540");
				return target;
			}
		},
		{
			"zx80", "ZX80",
			{
				{Kind::Choice, "memory", N_("Memory Size"), {{N_("Unexpanded"), 0}, {"16 KB", 16}, {"64 KB", 64}}, 0},
				{Kind::Toggle, "zx81-rom", N_("Use ZX81 ROM"), {}, 0},
			},
			[](const Values &values) {
				using Target = Analyser::Static::ZX8081::Target;
				auto target = std::make_unique<Target>();
				target->is_ZX81 = false;
				target->ZX80_uses_ZX81_ROM = values.flag("zx81-rom");
				switch(values["memory"]) {
					default:	target->memory_model = Target::MemoryModel::Unexpanded;		break;
					case 16:	target->memory_model = Target::MemoryModel::SixteenKB;		break;
					case 64:	target->memory_model = Target::MemoryModel::SixtyFourKB;	break;
				}
				return target;
			}
		},
		{
			"zx81", "ZX81",
			{
				{Kind::Choice, "memory", N_("Memory Size"), {{N_("Unexpanded"), 0}, {"16 KB", 16}, {"64 KB", 64}}, 16},
			},
			[](const Values &values) {
				using Target = Analyser::Static::ZX8081::Target;
				auto target = std::make_unique<Target>();
				target->is_ZX81 = true;
				switch(values["memory"]) {
					default:	target->memory_model = Target::MemoryModel::Unexpanded;		break;
					case 16:	target->memory_model = Target::MemoryModel::SixteenKB;		break;
					case 64:	target->memory_model = Target::MemoryModel::SixtyFourKB;	break;
				}
				return target;
			}
		},
		{
			"spectrum", "ZX Spectrum",
			{
				{Kind::Choice, "model", N_("Model"),
					{{"16K", 16}, {"48K", 48}, {"128K", 128}, {"+2", 2}, {"+2A", 21}, {"+3", 3}}, 21},
			},
			[](const Values &values) {
				using Target = Analyser::Static::ZXSpectrum::Target;
				auto target = std::make_unique<Target>();
				switch(values["model"]) {
					case 16:	target->model = Target::Model::SixteenK;		break;
					case 48:	target->model = Target::Model::FortyEightK;		break;
					case 128:	target->model = Target::Model::OneTwoEightK;	break;
					case 2:		target->model = Target::Model::Plus2;			break;
					default:	target->model = Target::Model::Plus2a;			break;
					case 3:		target->model = Target::Model::Plus3;			break;
				}
				return target;
			}
		},
	};
	return machines;
}

// MARK: - Stored choices.

constexpr const char *SettingsGroup = "new-machine";

std::string settings_key(const Machine &machine, const Option &option) {
	return std::string(machine.id) + "." + option.key;
}

// MARK: - The dialog.

struct Picker {
	Handler handler;
	AdwDialog *dialog = nullptr;
	GtkListBox *list = nullptr;
	GtkStack *pages = nullptr;

	// Per machine, per option: the widget that holds its value.
	std::vector<std::vector<GtkWidget *>> controls;
	std::map<GtkWidget *, std::string> folders;		// Folder rows and their chosen paths.

	size_t selected() const {
		GtkListBoxRow *const row = gtk_list_box_get_selected_row(list);
		return row ? size_t(gtk_list_box_row_get_index(row)) : 0;
	}

	Values values(const size_t index) const {
		Values result;
		const auto &machine = catalogue()[index];
		for(size_t c = 0; c < machine.options.size(); c++) {
			const auto &option = machine.options[c];
			GtkWidget *const control = controls[index][c];
			switch(option.kind) {
				case Kind::Choice: {
					const auto choice = adw_combo_row_get_selected(ADW_COMBO_ROW(control));
					result.numbers[option.key] =
						option.choices[std::min<size_t>(choice, option.choices.size() - 1)].value;
				} break;
				case Kind::Toggle:
					result.numbers[option.key] = adw_switch_row_get_active(ADW_SWITCH_ROW(control));
				break;
				case Kind::Folder:
					result.strings[option.key] = folders.at(control);
				break;
			}
		}
		return result;
	}

	void store(const size_t index) const {
		const auto &machine = catalogue()[index];
		const auto chosen = values(index);
		Settings::set_string(SettingsGroup, "machine", machine.id);
		for(const auto &option: machine.options) {
			if(option.kind == Kind::Folder) {
				Settings::set_string(SettingsGroup, settings_key(machine, option), chosen.strings.at(option.key));
			} else {
				Settings::set_int(SettingsGroup, settings_key(machine, option), chosen[option.key]);
			}
		}
		Settings::save();
	}

	void start() {
		const auto index = selected();
		store(index);

		const auto &machine = catalogue()[index];
		Analyser::Static::TargetList targets;
		targets.push_back(machine.make(values(index)));

		// Copy what's needed before the dialog, and therefore this object, goes away.
		const Handler handler = this->handler;
		const std::string name = _(machine.name);
		adw_dialog_close(dialog);
		handler(std::move(targets), name);
	}

	void set_folder(GtkWidget *const row, const std::string &path) {
		folders[row] = path;
		gchar *const display = path.empty() ? nullptr : g_filename_display_name(path.c_str());
		adw_action_row_set_subtitle(ADW_ACTION_ROW(row), display ? display : _("No Folder"));
		g_free(display);
	}

	GtkWidget *folder_row(const Option &option, const std::string &initial) {
		auto *const row = adw_action_row_new();
		adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), _(option.title));

		auto *const clear = gtk_button_new_from_icon_name("edit-clear-symbolic");
		gtk_widget_set_tooltip_text(clear, _("Don't Expose a Folder"));
		gtk_widget_set_valign(clear, GTK_ALIGN_CENTER);
		gtk_widget_add_css_class(clear, "flat");
		g_object_set_data(G_OBJECT(clear), "row", row);
		g_signal_connect(clear, "clicked", G_CALLBACK(+[](GtkButton *button, gpointer self) {
			auto *const row = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "row"));
			static_cast<Picker *>(self)->set_folder(row, "");
		}), this);
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), clear);

		auto *const choose = gtk_button_new_from_icon_name("folder-open-symbolic");
		gtk_widget_set_tooltip_text(choose, _("Choose Folder…"));
		gtk_widget_set_valign(choose, GTK_ALIGN_CENTER);
		gtk_widget_add_css_class(choose, "flat");
		g_object_set_data(G_OBJECT(choose), "row", row);
		g_signal_connect(choose, "clicked", G_CALLBACK(+[](GtkButton *button, gpointer data) {
			auto &self = *static_cast<Picker *>(data);
			auto *const row = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "row"));
			auto *const file_dialog = gtk_file_dialog_new();
			gtk_file_dialog_set_title(file_dialog, _("Choose Folder"));
			struct Request { Picker *self; GtkWidget *row; };
			gtk_file_dialog_select_folder(
				file_dialog,
				GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
				nullptr,
				[](GObject *source, GAsyncResult *result, gpointer data) {
					std::unique_ptr<Request> request(static_cast<Request *>(data));
					GFile *const folder = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), result, nullptr);
					if(!folder) return;
					if(gchar *const path = g_file_get_path(folder)) {
						request->self->set_folder(request->row, path);
						g_free(path);
					}
					g_object_unref(folder);
				},
				new Request{&self, row});
			g_object_unref(file_dialog);
		}), this);
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), choose);

		set_folder(row, initial);
		return row;
	}

	GtkWidget *page(const Machine &machine) {
		auto *const page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
		auto *const group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
		adw_preferences_group_set_title(group, _(machine.name));
		if(machine.note) {
			adw_preferences_group_set_description(group, _(machine.note));
		} else if(machine.options.empty()) {
			adw_preferences_group_set_description(group, _("This machine has no options."));
		}
		adw_preferences_page_add(page, group);

		std::vector<GtkWidget *> machine_controls;
		for(const auto &option: machine.options) {
			const auto key = settings_key(machine, option);
			GtkWidget *row = nullptr;
			switch(option.kind) {
				case Kind::Choice: {
					GtkStringList *const labels = gtk_string_list_new(nullptr);
					guint selection = 0;
					const int value = Settings::get_int(SettingsGroup, key).value_or(option.default_value);
					for(size_t c = 0; c < option.choices.size(); c++) {
						gtk_string_list_append(labels, _(option.choices[c].label));
						if(option.choices[c].value == value) selection = guint(c);
					}
					row = adw_combo_row_new();
					adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(labels));
					adw_combo_row_set_selected(ADW_COMBO_ROW(row), selection);
					g_object_unref(labels);
				} break;
				case Kind::Toggle:
					row = adw_switch_row_new();
					adw_switch_row_set_active(
						ADW_SWITCH_ROW(row),
						Settings::get_int(SettingsGroup, key).value_or(option.default_value) != 0);
				break;
				case Kind::Folder:
					row = folder_row(option, Settings::get_string(SettingsGroup, key).value_or(""));
				break;
			}
			if(option.kind != Kind::Folder) {
				adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
				adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), _(option.title));
			}
			adw_preferences_group_add(group, row);
			machine_controls.push_back(row);
		}
		controls.push_back(std::move(machine_controls));
		return GTK_WIDGET(page);
	}

	void build(GtkWidget *const parent) {
		dialog = adw_dialog_new();
		adw_dialog_set_title(dialog, _("New Machine"));
		adw_dialog_set_content_width(dialog, 760);
		adw_dialog_set_content_height(dialog, 520);

		auto *const header = ADW_HEADER_BAR(adw_header_bar_new());
		adw_header_bar_set_show_start_title_buttons(header, FALSE);
		adw_header_bar_set_show_end_title_buttons(header, FALSE);
		auto *const cancel = gtk_button_new_with_mnemonic(_("_Cancel"));
		g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(adw_dialog_close), dialog);
		adw_header_bar_pack_start(header, cancel);
		auto *const start_button = gtk_button_new_with_mnemonic(_("_Start"));
		gtk_widget_add_css_class(start_button, "suggested-action");
		g_signal_connect(start_button, "clicked", G_CALLBACK(+[](GtkButton *, gpointer self) {
			static_cast<Picker *>(self)->start();
		}), this);
		adw_header_bar_pack_end(header, start_button);

		// Machines on the left, the selected machine's options on the right.
		list = GTK_LIST_BOX(gtk_list_box_new());
		gtk_widget_add_css_class(GTK_WIDGET(list), "navigation-sidebar");
		gtk_list_box_set_selection_mode(list, GTK_SELECTION_BROWSE);
		gtk_list_box_set_activate_on_single_click(list, FALSE);
		pages = GTK_STACK(gtk_stack_new());
		gtk_widget_set_hexpand(GTK_WIDGET(pages), TRUE);

		const auto stored = Settings::get_string(SettingsGroup, "machine");
		GtkListBoxRow *initial = nullptr;
		for(const auto &machine: catalogue()) {
			auto *const label = gtk_label_new(_(machine.name));
			gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
			gtk_list_box_append(list, label);
			gtk_stack_add_named(pages, page(machine), machine.id);
			if(stored && *stored == machine.id) {
				initial = gtk_list_box_get_row_at_index(list, int(controls.size() - 1));
			}
		}

		g_signal_connect(list, "row-selected", G_CALLBACK(+[](GtkListBox *, GtkListBoxRow *row, gpointer self) {
			if(!row) return;
			auto &picker = *static_cast<Picker *>(self);
			gtk_stack_set_visible_child_name(picker.pages, catalogue()[size_t(gtk_list_box_row_get_index(row))].id);
		}), this);
		g_signal_connect(list, "row-activated", G_CALLBACK(+[](GtkListBox *, GtkListBoxRow *, gpointer self) {
			static_cast<Picker *>(self)->start();
		}), this);

		auto *const sidebar = gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar), GTK_WIDGET(list));
		gtk_widget_set_size_request(sidebar, 200, -1);

		auto *const body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_append(GTK_BOX(body), sidebar);
		gtk_box_append(GTK_BOX(body), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
		gtk_box_append(GTK_BOX(body), GTK_WIDGET(pages));

		auto *const toolbar = adw_toolbar_view_new();
		adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(header));
		adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), body);
		adw_dialog_set_child(dialog, toolbar);
		adw_dialog_set_default_widget(dialog, start_button);

		if(!initial) initial = gtk_list_box_get_row_at_index(list, 0);
		gtk_list_box_select_row(list, initial);

		// The picker lives as long as its dialog.
		g_signal_connect(dialog, "closed", G_CALLBACK(+[](AdwDialog *, gpointer self) {
			delete static_cast<Picker *>(self);
		}), this);
		adw_dialog_present(dialog, parent);

		// Start with the machine list focussed, so the keyboard can pick a machine and Enter can start it.
		adw_dialog_set_focus(dialog, GTK_WIDGET(initial));
	}
};

}

void present(GtkWidget *const parent, Handler handler) {
	auto *const picker = new Picker;
	picker->handler = std::move(handler);
	picker->build(parent);
}

}
