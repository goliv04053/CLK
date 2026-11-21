//
//  AudioOutput.hpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#pragma once

#include "../../Outputs/Speaker/Speaker.hpp"

#include <SDL.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace GTKFrontend {

/// Plays a machine's speaker through SDL's audio subsystem (PipeWire or PulseAudio on Fedora),
/// keeping only a small amount of audio queued to minimise latency.
class AudioOutput: public Outputs::Speaker::Speaker::Delegate {
public:
	~AudioOutput();

	/// Opens an output device and attaches to @c speaker. Returns @c false if no audio is available.
	bool start(Outputs::Speaker::Speaker &speaker);
	void stop();

	/// Volume in the range [0, 1].
	void set_volume(float);

private:
	void speaker_did_complete_samples(Outputs::Speaker::Speaker &, const std::vector<int16_t> &) override;
	void speaker_did_change_input_clock(Outputs::Speaker::Speaker &) override;
	static void callback(void *, Uint8 *, int);

	Outputs::Speaker::Speaker *speaker_ = nullptr;
	SDL_AudioDeviceID device_ = 0;
	int sample_rate_ = 0;
	int channels_ = 1;
	int buffer_samples_ = 512;

	std::mutex buffer_mutex_;
	std::vector<int16_t> buffer_;
};

}
