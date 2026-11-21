//
//  MachineRunner.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include "../../Machines/TimedMachine.hpp"

#include <atomic>
#include <mutex>
#include <thread>

namespace GTKFrontend {

/// Runs a timed machine in real time on a dedicated thread, in small slices,
/// holding the supplied mutex while the machine runs.
class MachineRunner {
public:
	~MachineRunner();

	void start(MachineTypes::TimedMachine *, std::mutex *);
	void stop();

	/// Pausing keeps the thread alive but stops advancing emulated time.
	void set_paused(bool);
	bool paused() const { return paused_; }

private:
	void run();

	MachineTypes::TimedMachine *machine_ = nullptr;
	std::mutex *mutex_ = nullptr;
	std::thread thread_;
	std::atomic<bool> running_ = false;
	std::atomic<bool> paused_ = false;
};

}
