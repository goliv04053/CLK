//
//  MachineRunner.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "MachineRunner.hpp"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <algorithm>
#include <chrono>

using namespace GTKFrontend;

MachineRunner::~MachineRunner() {
	stop();
}

void MachineRunner::start(MachineTypes::TimedMachine *const machine, std::mutex *const mutex) {
	stop();
	machine_ = machine;
	mutex_ = mutex;
	running_ = true;
	thread_ = std::thread([this] { run(); });
}

void MachineRunner::stop() {
	running_ = false;
	if(thread_.joinable()) thread_.join();
}

void MachineRunner::set_paused(const bool paused) {
	paused_ = paused;
}

void MachineRunner::run() {
	// Same approach as the Qt frontend: advance the machine by the real time elapsed
	// since the last slice, at roughly millisecond granularity, discarding any gap of
	// more than half a second (e.g. after the host was suspended).
	auto last = Time::nanos_now();
	while(running_) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		const auto now = Time::nanos_now();
		const auto duration = std::min(now - last, Time::Nanos(500'000'000));
		last = now;
		if(paused_) continue;

		std::lock_guard lock(*mutex_);
		machine_->run_for(double(duration) / 1e9);
		machine_->flush_output(MachineTypes::TimedMachine::Output::All);
	}
}
