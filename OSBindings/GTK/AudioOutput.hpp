//
//  AudioOutput.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include "../../Outputs/Speaker/Speaker.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

struct pa_simple;

namespace GTKFrontend {

/// Plays a machine's speaker through PulseAudio, which on Fedora and other current
/// desktops is served by PipeWire. Only a small amount of audio is kept queued, to
/// minimise latency.
class AudioOutput: public Outputs::Speaker::Speaker::Delegate {
public:
	~AudioOutput();

	/// Opens an output stream and attaches to @c speaker. Returns @c false if no audio is available.
	bool start(Outputs::Speaker::Speaker &speaker);
	void stop();

private:
	void speaker_did_complete_samples(Outputs::Speaker::Speaker &, const std::vector<int16_t> &) override;
	void speaker_did_change_input_clock(Outputs::Speaker::Speaker &) override;
	void run();

	static constexpr int SampleRate = 48000;
	static constexpr int BufferFrames = 512;

	Outputs::Speaker::Speaker *speaker_ = nullptr;
	pa_simple *stream_ = nullptr;
	int channels_ = 1;

	std::thread thread_;
	std::atomic<bool> running_ = false;
	std::mutex buffer_mutex_;
	std::condition_variable buffer_condition_;
	std::vector<int16_t> buffer_;
};

}
