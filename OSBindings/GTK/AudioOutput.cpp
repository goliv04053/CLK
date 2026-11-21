//
//  AudioOutput.cpp
//  Clock Signal GTK frontend
//
//  Unofficial GTK4/libadwaita frontend for Clock Signal, by Thomas Harte.
//  MIT licence; see LICENCE at the repository root.
//

#include "AudioOutput.hpp"

#include <algorithm>
#include <cstring>

using namespace GTKFrontend;

AudioOutput::~AudioOutput() {
	stop();
}

bool AudioOutput::start(Outputs::Speaker::Speaker &speaker) {
	stop();

	if(!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		return false;
	}

	SDL_AudioSpec desired{}, obtained{};
	desired.freq = 48000;
	desired.format = AUDIO_S16SYS;
	desired.channels = Uint8(speaker.get_is_stereo() ? 2 : 1);
	desired.samples = Uint16(buffer_samples_);
	desired.callback = &AudioOutput::callback;
	desired.userdata = this;

	device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	if(!device_) return false;

	sample_rate_ = obtained.freq;
	channels_ = obtained.channels;
	buffer_samples_ = obtained.samples;

	speaker_ = &speaker;
	speaker.set_output_rate(float(sample_rate_), buffer_samples_, channels_ == 2);
	speaker.set_delegate(this);
	SDL_PauseAudioDevice(device_, 0);
	return true;
}

void AudioOutput::stop() {
	if(device_) {
		SDL_CloseAudioDevice(device_);
		device_ = 0;
	}
	if(speaker_) {
		speaker_->set_delegate(nullptr);
		speaker_ = nullptr;
	}
	std::lock_guard lock(buffer_mutex_);
	buffer_.clear();
}

void AudioOutput::set_volume(const float volume) {
	if(speaker_) speaker_->set_output_volume(std::clamp(volume, 0.0f, 1.0f));
}

void AudioOutput::speaker_did_complete_samples(Outputs::Speaker::Speaker &, const std::vector<int16_t> &samples) {
	std::lock_guard lock(buffer_mutex_);

	// Keep at most two device buffers queued; drop the oldest audio if the emulator
	// runs ahead, so latency can't grow without bound.
	const size_t limit = size_t(buffer_samples_ * channels_ * 2);
	buffer_.insert(buffer_.end(), samples.begin(), samples.end());
	if(buffer_.size() > limit) {
		buffer_.erase(buffer_.begin(), buffer_.end() - ptrdiff_t(limit));
	}
}

void AudioOutput::speaker_did_change_input_clock(Outputs::Speaker::Speaker &speaker) {
	speaker.set_output_rate(float(sample_rate_), buffer_samples_, channels_ == 2);
}

void AudioOutput::callback(void *const user_data, Uint8 *const stream, const int length) {
	auto &self = *static_cast<AudioOutput *>(user_data);
	std::lock_guard lock(self.buffer_mutex_);

	const size_t wanted = size_t(length) / sizeof(int16_t);
	const size_t available = std::min(wanted, self.buffer_.size());
	std::memcpy(stream, self.buffer_.data(), available * sizeof(int16_t));
	std::memset(stream + available * sizeof(int16_t), 0, (wanted - available) * sizeof(int16_t));
	self.buffer_.erase(self.buffer_.begin(), self.buffer_.begin() + ptrdiff_t(available));
}
