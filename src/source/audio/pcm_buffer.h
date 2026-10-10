#pragma once

#include "audio/directsound.h"

#include <cstddef>
#include <cstdint>

namespace sea::audio {

	// A lock on the entire DirectSound buffer of a game sound, with 16-bit PCM samples.
	//
	// Samples are interleaved by channel.
	struct PcmLock {
		IDirectSoundBuffer8* buffer = nullptr;
		WAVEFORMATEX format{};

		void* audio = nullptr;
		DWORD audioBytes = 0;
		void* wrappedAudio = nullptr;
		DWORD wrappedBytes = 0;

		std::int16_t* Samples() const {
			return static_cast<std::int16_t*>(audio);
		}

		std::size_t FrameCount() const {
			return audioBytes / format.nBlockAlign;
		}
	};

	enum class PcmLockResult {
		Locked,
		NoBuffer,
		UnsupportedFormat,
		Failed,
	};

	PcmLockResult LockPcmBuffer(void* gameSound, PcmLock& lock);

	void UnlockPcmBuffer(PcmLock& lock);

}
