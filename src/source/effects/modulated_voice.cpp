#include "effects/modulated_voice.h"

#include "audio/directsound.h"
#include "config/settings.h"
#include "effects/mask_filter.h"
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
		constexpr std::size_t kMaxTrackedBuffers = 512;

		// Content hash of each buffer after filtering.
		// The same hash at the next Play means that the engine did not write new data, so the buffer is already filtered.
		std::mutex g_lock;
		std::unordered_map<std::uint32_t, std::uint64_t> g_filteredContent;

		bool g_unsupportedFormatLogged = false;  // Audio thread only.

		// FNV-1a, 64 bit.
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

		bool ShouldFilter(void* gameSound) {
			if (!config::Get().mask.enabled) {
				return false;
			}

			const std::uint32_t flags = Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags);

			if (!(flags & engine::kSound_Modulated)) {
				return false;
			}

			if (!audio::IsDsoalLoaded()) {
				return false;
			}

			return !reverb::IsBypassed();
		}

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
			SEA_LOG("[Mask] Format is not supported: Tag=%u Bits=%u Channels=%u. Only 16-bit PCM is filtered.",
				format.wFormatTag, format.wBitsPerSample, format.nChannels);
		}
	}

	void ProcessModulatedVoice(void* gameSound) {
		if (!ShouldFilter(gameSound)) {
			return;
		}

		const std::uint32_t bufferAddress = Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer);

		if (!audio::IsDSoundObject(bufferAddress)) {
			return;
		}

		auto* buffer = reinterpret_cast<IDirectSoundBuffer8*>(bufferAddress);
		WAVEFORMATEX format{};

		if (FAILED(buffer->GetFormat(&format, sizeof(format), nullptr))) {
			LogUnsupportedFormat(format);

			return;
		}

		if (!IsSupportedFormat(format)) {
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

		if (IsAlreadyFiltered(bufferAddress, HashContent(audio, audioBytes))) {
			buffer->Unlock(audio, audioBytes, wrappedAudio, wrappedBytes);

			return;
		}

		const std::size_t frameCount = audioBytes / format.nBlockAlign;
		const double gainDb = ApplyMaskFilter(static_cast<std::int16_t*>(audio), frameCount, format.nChannels,
			static_cast<double>(format.nSamplesPerSec), config::Get().mask);

		RememberFiltered(bufferAddress, HashContent(audio, audioBytes));
		buffer->Unlock(audio, audioBytes, wrappedAudio, wrappedBytes);

		const double durationSeconds = static_cast<double>(frameCount) / format.nSamplesPerSec;

		SEA_LOG("[Mask] Filtered %p: %lu Hz, %u ch, %.2f s, Gain %+.1f dB, Path=\"%.200s\"", gameSound,
			format.nSamplesPerSec, format.nChannels, durationSeconds, gainDb, &Field<char>(gameSound, engine::kSound_FilePath));
	}
}
