#include "effects/voice_filter.h"

#include "audio/directsound.h"
#include "config/settings.h"
#include "effects/mask_filter.h"
#include "effects/voice_cover.h"
#include "engine/addresses.h"
#include "engine/sound_flags.h"
#include "reverb/reverb.h"
#include "utils/log.h"
#include "utils/memory.h"

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
		std::unordered_map<std::uint32_t, std::uint64_t> g_filteredContent;

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

		// Returns the FNV-1a hash (64 bit) of a block of memory.
		std::uint64_t HashContent(const void* data, std::size_t size) {
			const auto* bytes = static_cast<const std::uint8_t*>(data);
			std::uint64_t hash = 0xCBF29CE484222325ull;

			for (std::size_t index = 0; index < size; ++index) {
				hash ^= bytes[index];
				hash *= 0x100000001B3ull;
			}

			return hash;
		}

		bool IsAlreadyFiltered(std::uint32_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);
			const auto entry = g_filteredContent.find(buffer);

			if (entry == g_filteredContent.end()) {
				return false;
			}

			return entry->second == contentHash;
		}

		void RememberFiltered(std::uint32_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);

			// Released buffers are never removed, so the map is cleared when it grows too large.
			if (g_filteredContent.size() >= kMaxTrackedBuffers) {
				g_filteredContent.clear();
			}

			g_filteredContent[buffer] = contentHash;
		}

		// Checks if the format is 16-bit PCM with packed samples.
		bool IsSupportedFormat(const WAVEFORMATEX& format) {
			if (format.wFormatTag != WAVE_FORMAT_PCM || format.wBitsPerSample != 16 || format.nChannels == 0) {
				return false;
			}

			return format.nBlockAlign == format.nChannels * 2;
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
		if (!config::Get().voiceFilters.enabled || reverb::IsBypassed() || !audio::IsDsoalLoaded()) {
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

		const std::uint32_t bufferAddress = Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer);

		if (!audio::IsDSoundObject(bufferAddress)) {
			return;
		}

		auto* buffer = reinterpret_cast<IDirectSoundBuffer8*>(bufferAddress);
		WAVEFORMATEX format{};

		if (FAILED(buffer->GetFormat(&format, sizeof(format), nullptr)) || !IsSupportedFormat(format)) {
			LogUnsupportedFormat(format);

			return;
		}

		void* audio = nullptr;
		DWORD audioBytes = 0;
		void* wrappedAudio = nullptr;
		DWORD wrappedBytes = 0;

		if (FAILED(buffer->Lock(0, 0, &audio, &audioBytes, &wrappedAudio, &wrappedBytes, DSBLOCK_ENTIREBUFFER)) || !audio) {
			return;
		}

		// Engine reuses buffers. Filter only if the content changed since the last filter.
		if (IsAlreadyFiltered(bufferAddress, HashContent(audio, audioBytes))) {
			buffer->Unlock(audio, audioBytes, wrappedAudio, wrappedBytes);

			return;
		}

		const std::size_t frameCount = audioBytes / format.nBlockAlign;
		const double gainDb = ApplyMaskFilter(static_cast<std::int16_t*>(audio), frameCount, format.nChannels,
			static_cast<double>(format.nSamplesPerSec), *preset);

		RememberFiltered(bufferAddress, HashContent(audio, audioBytes));
		buffer->Unlock(audio, audioBytes, wrappedAudio, wrappedBytes);

		const double durationSeconds = static_cast<double>(frameCount) / format.nSamplesPerSec;

		SEA_LOG("[Voice] Filtered %p as %s: %lu Hz, %u ch, %.2f s, Gain %+.1f dB, Path=\"%.200s\"", gameSound,
			VoiceCoverName(cover), format.nSamplesPerSec, format.nChannels, durationSeconds, gainDb,
			&Field<char>(gameSound, engine::kSound_FilePath));
	}

}
