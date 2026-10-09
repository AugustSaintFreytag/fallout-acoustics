#include "effects/voice_filter.h"

#include "audio/directsound.h"
#include "audio/pcm_buffer.h"
#include "config/settings.h"
#include "effects/mask_filter.h"
#include "effects/voice_cover.h"
#include "engine/addresses.h"
#include "engine/sound_flags.h"
#include "reverb/reverb.h"
#include "utils/hash.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace sea::effects {

	using mem::Field;

	namespace {

		// Light mask (cloth, face cover, surgical mask, etc.)
		constexpr MaskFilterPreset kLightCoverPreset{
			.lowCutFrequency = 0.0,
			.highCutFrequency = 4000.0,
			.resonanceFrequency = 0.0,
			.resonanceGainLevel = 0.0,
			.presenceFrequency = 0.0,
			.presenceGainLevel = 0.0,
			.steepHighCut = false,
			.driveLevel = 0.0,
			.gainLevel = 1.0,
		};

		// Heavy mask, full head cover (gas mask, closed helmet, etc.)
		constexpr MaskFilterPreset kFullCoverPreset{
			.lowCutFrequency = 180.0,
			.highCutFrequency = 2300.0,
			.resonanceFrequency = 650.0,
			.resonanceGainLevel = 6.0,
			.presenceFrequency = 1500.0,
			.presenceGainLevel = 2.0,
			.steepHighCut = true,
			.driveLevel = 0.0,
			.gainLevel = 3.0,
		};

		// Electric speaker (power armor helmet, intercom, etc.)
		constexpr MaskFilterPreset kSpeakerPreset{
			.lowCutFrequency = 350.0,
			.highCutFrequency = 3400.0,
			.resonanceFrequency = 1000.0,
			.resonanceGainLevel = 4.0,
			.presenceFrequency = 2500.0,
			.presenceGainLevel = 3.0,
			.steepHighCut = true,
			.driveLevel = 6.0,
			.gainLevel = 1.0,
		};

		constexpr std::size_t kMaxTrackedBuffers = 512;

		// Content hash of each buffer after filtering.
		// The same hash at the next Play means that the engine did not write new data, 
		// so buffer can be assumed to already be filtered.
		std::mutex g_lock;
		std::unordered_map<std::uintptr_t, std::uint64_t> g_filteredContent;

		// Thread: Audio
		bool g_unsupportedFormatLogged = false;

		const MaskFilterPreset* PresetFor(VoiceCover cover) {
			switch (cover) {
			case VoiceCover::Light:
				return &kLightCoverPreset;

			case VoiceCover::Full:
				return &kFullCoverPreset;

			case VoiceCover::Speaker:
				return &kSpeakerPreset;

			default:
				return nullptr;
			}
		}

		bool IsAlreadyFiltered(std::uintptr_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);
			const auto entry = g_filteredContent.find(buffer);

			if (entry == g_filteredContent.end()) {
				return false;
			}

			return entry->second == contentHash;
		}

		void RememberFiltered(std::uintptr_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);

			// Released buffers are never removed, so the map is cleared when it grows too large.
			if (g_filteredContent.size() >= kMaxTrackedBuffers) {
				g_filteredContent.clear();
			}

			g_filteredContent[buffer] = contentHash;
		}

		void LogUnsupportedFormat(const WAVEFORMATEX& format) {
			if (g_unsupportedFormatLogged) {
				return;
			}

			g_unsupportedFormatLogged = true;
			SEA_LOG("[Voice] Format is not supported: Tag=%u Bits=%u Channels=%u. Only 16-bit PCM is filtered.",
				format.wFormatTag, format.wBitsPerSample, format.nChannels);
		}

	}

	// Filters the buffer of a covered voice in place, with the preset of its cover. 
	// Called before the original `Play`.
	// DSOAL only: native DirectSound applies the vanilla effect.
	//
	// Thread: Audio
	void ProcessVoiceFilter(void* gameSound) {
		if (!config::Get().voiceFilters.enabled || reverb::IsBypassed() || !audio::IsDSOALLoaded()) {
			return;
		}

		const std::uint32_t soundFlags = Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags);

		// Radio songs can be streamed, and the filter needs the complete sound in the buffer.
		if (soundFlags & engine::kSound_Radio) {
			return;
		}

		const VoiceCover cover = VoiceCoverFromFlags(soundFlags);
		const MaskFilterPreset* preset = PresetFor(cover);

		if (!preset) {
			return;
		}

		audio::PcmLock lock;
		const audio::PcmLockResult lockResult = audio::LockPcmBuffer(gameSound, lock);

		if (lockResult == audio::PcmLockResult::UnsupportedFormat) {
			LogUnsupportedFormat(lock.format);
		}

		if (lockResult != audio::PcmLockResult::Locked) {
			return;
		}

		const auto bufferAddress = reinterpret_cast<std::uintptr_t>(lock.buffer);

		// Engine reuses buffers. Filter only if the content changed since the last filter.
		if (IsAlreadyFiltered(bufferAddress, hash::Fnv1a(lock.audio, lock.audioBytes))) {
			audio::UnlockPcmBuffer(lock);

			return;
		}

		const WAVEFORMATEX& format = lock.format;
		const std::size_t frameCount = lock.FrameCount();
		const double gainDb = ApplyMaskFilter(lock.Samples(), frameCount, format.nChannels,
			static_cast<double>(format.nSamplesPerSec), *preset);

		RememberFiltered(bufferAddress, hash::Fnv1a(lock.audio, lock.audioBytes));
		audio::UnlockPcmBuffer(lock);

		const double durationSeconds = static_cast<double>(frameCount) / format.nSamplesPerSec;

		SEA_LOG("[Voice] Filtered %p as %s: %lu Hz, %u ch, %.2f s, Gain %+.1f dB, Path=\"%.200s\"", gameSound,
			VoiceCoverName(cover), format.nSamplesPerSec, format.nChannels, durationSeconds, gainDb,
			&Field<char>(gameSound, engine::kSound_FilePath));
	}

}
