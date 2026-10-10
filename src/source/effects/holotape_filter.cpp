#include "effects/holotape_filter.h"

#include "audio/directsound.h"
#include "audio/pcm_buffer.h"
#include "config/settings.h"
#include "effects/filtered_buffers.h"
#include "effects/tape_filter.h"
#include "engine/addresses.h"
#include "engine/holotapes.h"
#include "reverb/reverb.h"
#include "utils/hash.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstddef>
#include <cstdint>

namespace sea::effects {

	using mem::Field;

	namespace {

		// Holotape played through the Pip-Boy speaker.
		constexpr TapeFilterPreset kHolotapePreset{
			.lowCutFrequency = 120.0,
			.highCutFrequency = 4800.0,
			.headBumpFrequency = 180.0,
			.headBumpGainLevel = 3.0,
			.driveLevel = 4.0,
			.hissLevel = -42.0,
			.wowRate = 0.7,
			.wowDepth = 0.0004,
			.flutterRate = 6.5,
			.flutterDepth = 0.00003,
			.gainLevel = 0.0,
		};

		// Share of the sound duration the buffer must hold. A shorter buffer is streamed.
		constexpr double kMinBufferShare = 0.9;

		// Thread: Audio
		FilteredBuffers g_filteredBuffers;
		bool g_streamedBufferLogged = false;

		// Checks if the locked buffer holds less than the full sound. Unknown durations count as complete.
		bool IsStreamedBuffer(void* gameSound, const audio::PcmLock& lock) {
			const std::uint32_t durationMs = Field<std::uint32_t>(gameSound, engine::kSound_Duration);

			if (durationMs == 0) {
				return false;
			}

			const double bufferMs = static_cast<double>(lock.FrameCount()) * 1000.0 / lock.format.nSamplesPerSec;

			return bufferMs < durationMs * kMinBufferShare;
		}

	}

	// Filters the buffer of a holotape line in place, like playback from an old tape.
	// Called before the original `Play`. Skips buffers that do not hold the full sound.
	// DSOAL only, like the voice filter.
	//
	// Thread: Audio
	void ProcessHolotapeFilter(void* gameSound) {
		if (!config::Get().vocals.holotapeFilter || reverb::IsBypassed() || !audio::IsDSOALLoaded()) {
			return;
		}

		if (!engine::IsHolotapeSound(gameSound)) {
			return;
		}

		const char* path = &Field<char>(gameSound, engine::kSound_FilePath);

		audio::PcmLock lock;

		if (audio::LockPcmBuffer(gameSound, lock) != audio::PcmLockResult::Locked) {
			return;
		}

		if (IsStreamedBuffer(gameSound, lock)) {
			audio::UnlockPcmBuffer(lock);

			if (!g_streamedBufferLogged) {
				g_streamedBufferLogged = true;
				SEA_LOG("[Holotape] Buffer of %p holds less than the full sound. Not filtered. Path=\"%.200s\"", gameSound, path);
			}

			return;
		}

		const auto bufferAddress = reinterpret_cast<std::uintptr_t>(lock.buffer);

		if (g_filteredBuffers.Contains(bufferAddress, hash::Fnv1a(lock.audio, lock.audioBytes))) {
			audio::UnlockPcmBuffer(lock);

			return;
		}

		const WAVEFORMATEX& format = lock.format;
		const std::size_t frameCount = lock.FrameCount();
		const double gainDb = ApplyTapeFilter(lock.Samples(), frameCount, format.nChannels,
			static_cast<double>(format.nSamplesPerSec), kHolotapePreset);

		g_filteredBuffers.Remember(bufferAddress, hash::Fnv1a(lock.audio, lock.audioBytes));
		audio::UnlockPcmBuffer(lock);

		const double durationSeconds = static_cast<double>(frameCount) / format.nSamplesPerSec;

		SEA_LOG("[Holotape] Filtered %p: %lu Hz, %u ch, %.2f s, Gain %+.1f dB, Path=\"%.200s\"", gameSound,
			format.nSamplesPerSec, format.nChannels, durationSeconds, gainDb, path);
	}

}
