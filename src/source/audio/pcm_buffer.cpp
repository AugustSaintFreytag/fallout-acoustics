#include "audio/pcm_buffer.h"

#include "engine/addresses.h"
#include "utils/memory.h"

namespace sea::audio {

	using mem::Field;

	namespace {

		// Checks if the format is 16-bit PCM with packed samples.
		bool IsSupportedFormat(const WAVEFORMATEX& format) {
			if (format.wFormatTag != WAVE_FORMAT_PCM || format.wBitsPerSample != 16 || format.nChannels == 0) {
				return false;
			}

			return format.nBlockAlign == format.nChannels * 2;
		}

	}

	// Locks the entire DirectSound buffer of the given game sound for reading and writing samples.
	// The lock gets the buffer format in all cases where the buffer exists, also for an unsupported format.
	// Returns `Locked` only if the buffer is 16-bit PCM and the lock succeeded. Only then it needs `UnlockPcmBuffer`.
	PcmLockResult LockPcmBuffer(void* gameSound, PcmLock& lock) {
		const std::uint32_t bufferAddress = Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer);

		if (!IsDSoundObject(bufferAddress)) {
			return PcmLockResult::NoBuffer;
		}

		lock.buffer = reinterpret_cast<IDirectSoundBuffer8*>(bufferAddress);

		if (FAILED(lock.buffer->GetFormat(&lock.format, sizeof(lock.format), nullptr)) || !IsSupportedFormat(lock.format)) {
			return PcmLockResult::UnsupportedFormat;
		}

		if (FAILED(lock.buffer->Lock(0, 0, &lock.audio, &lock.audioBytes, &lock.wrappedAudio, &lock.wrappedBytes,
				DSBLOCK_ENTIREBUFFER)) || !lock.audio) {
			return PcmLockResult::Failed;
		}

		return PcmLockResult::Locked;
	}

	void UnlockPcmBuffer(PcmLock& lock) {
		lock.buffer->Unlock(lock.audio, lock.audioBytes, lock.wrappedAudio, lock.wrappedBytes);
		lock.audio = nullptr;
	}

}
