//
//  AudioOutput.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "AudioOutput.hpp"

#include <pulse/simple.h>

#include <algorithm>
#include <chrono>

using namespace GTKFrontend;

AudioOutput::~AudioOutput() {
	stop();
}

bool AudioOutput::start(Outputs::Speaker::Speaker &speaker) {
	stop();

	channels_ = speaker.get_is_stereo() ? 2 : 1;
	const pa_sample_spec spec = {PA_SAMPLE_S16NE, uint32_t(SampleRate), uint8_t(channels_)};

	// Ask the server to keep about two buffers' worth queued: enough to ride out scheduling
	// jitter, little enough that sound stays in step with the picture.
	const uint32_t buffer_bytes = uint32_t(BufferFrames * channels_ * sizeof(int16_t));
	pa_buffer_attr attributes;
	attributes.maxlength = uint32_t(-1);
	attributes.tlength = buffer_bytes * 2;
	attributes.prebuf = uint32_t(-1);
	attributes.minreq = buffer_bytes / 2;
	attributes.fragsize = uint32_t(-1);

	int error = 0;
	stream_ = pa_simple_new(nullptr, "Clock Signal", PA_STREAM_PLAYBACK, nullptr, "Emulation", &spec, nullptr, &attributes, &error);
	if(!stream_) return false;

	speaker_ = &speaker;
	speaker.set_output_rate(float(SampleRate), BufferFrames, channels_ == 2);
	speaker.set_delegate(this);

	running_ = true;
	thread_ = std::thread([this] { run(); });
	return true;
}

void AudioOutput::stop() {
	if(running_) {
		running_ = false;
		buffer_condition_.notify_all();
	}
	if(thread_.joinable()) thread_.join();

	if(speaker_) {
		speaker_->set_delegate(nullptr);
		speaker_ = nullptr;
	}
	if(stream_) {
		pa_simple_free(stream_);
		stream_ = nullptr;
	}
	std::lock_guard lock(buffer_mutex_);
	buffer_.clear();
}

void AudioOutput::speaker_did_complete_samples(Outputs::Speaker::Speaker &, const std::vector<int16_t> &samples) {
	{
		std::lock_guard lock(buffer_mutex_);
		buffer_.insert(buffer_.end(), samples.begin(), samples.end());

		// If the emulator has run ahead of the audio device, drop the oldest audio
		// rather than let latency build up.
		const size_t limit = size_t(BufferFrames * channels_ * 4);
		if(buffer_.size() > limit) {
			buffer_.erase(buffer_.begin(), buffer_.end() - ptrdiff_t(limit));
		}
	}
	buffer_condition_.notify_one();
}

void AudioOutput::speaker_did_change_input_clock(Outputs::Speaker::Speaker &speaker) {
	speaker.set_output_rate(float(SampleRate), BufferFrames, channels_ == 2);
}

void AudioOutput::run() {
	const size_t chunk = size_t(BufferFrames * channels_);
	std::vector<int16_t> output(chunk);

	while(running_) {
		{
			std::unique_lock lock(buffer_mutex_);
			buffer_condition_.wait_for(lock, std::chrono::milliseconds(20), [&] {
				return !running_ || buffer_.size() >= chunk;
			});
			if(!running_) break;

			// Play whatever has arrived; pad with silence if the machine is paused or slow.
			const size_t available = std::min(chunk, buffer_.size());
			std::copy_n(buffer_.begin(), available, output.begin());
			std::fill(output.begin() + ptrdiff_t(available), output.end(), int16_t(0));
			buffer_.erase(buffer_.begin(), buffer_.begin() + ptrdiff_t(available));
		}

		// Blocks while the server's buffer is full, which paces this thread.
		int error = 0;
		if(pa_simple_write(stream_, output.data(), output.size() * sizeof(int16_t), &error) < 0) {
			break;
		}
	}
}
