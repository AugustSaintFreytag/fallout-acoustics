#include "effects/gunshot_filter.h"

#include "audio/directsound.h"
#include "audio/pcm_buffer.h"
#include "config/settings.h"
#include "effects/deverb.h"
#include "engine/addresses.h"
#include "engine/gunfire_path.h"
#include "engine/sound_flags.h"
#include "reverb/reverb.h"
#include "utils/hash.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>

namespace sea::effects {

	using mem::Field;

	namespace {

		constexpr std::size_t kMaxHandledContents = 4096;

		// Sounds that are never a single gunshot, whatever their path.
		constexpr std::uint32_t kExcludedFlags = engine::kSound_Loop | engine::kSound_Voice | engine::kSound_Music |
			engine::kSound_Radio | engine::kSound_SystemSound;

		// Content hashes of buffers already handled, deverbed or rejected.
		// The engine reuses buffers and can share sample memory between them. A known hash is skipped without analysis.
		// A deverbed buffer is stored with its new content. A second deverb would shorten it again.
		std::mutex g_lock;
		std::unordered_set<std::uint64_t> g_handledContents;

		// Thread: Audio
		bool g_unsupportedFormatLogged = false;

		bool IsHandled(std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);

			return g_handledContents.contains(contentHash);
		}

		void RememberHandled(std::uint64_t contentHash) {
			std::lock_guard guard(g_lock);

			// Hashes are never removed, so the set is cleared when it grows too large.
			if (g_handledContents.size() >= kMaxHandledContents) {
				g_handledContents.clear();
			}

			g_handledContents.insert(contentHash);
		}

		DeverbParameters ParametersFromSettings(const config::GunshotSettings& gunshots) {
			return DeverbParameters{
				.keepFraction = gunshots.keepFraction,
				.fadeThreshold = gunshots.fadeThreshold,
				.maxFadeDelay = gunshots.maxFadeDelay,
				.decayRate = gunshots.decayRate,
				.repeatLevel = gunshots.repeatLevel,
			};
		}

		double FramesToMilliseconds(std::size_t frames, const WAVEFORMATEX& format) {
			return static_cast<double>(frames) * 1000.0 / format.nSamplesPerSec;
		}

		void LogUnsupportedFormat(const WAVEFORMATEX& format, const char* path) {
			if (g_unsupportedFormatLogged) {
				return;
			}

			g_unsupportedFormatLogged = true;
			SEA_LOG("[Gunshot] Format is not supported: Tag=%u Bits=%u Channels=%u. Only 16-bit PCM is deverbed. "
					"Path=\"%.200s\"",
				format.wFormatTag, format.wBitsPerSample, format.nChannels, path);
		}

		void LogReport(void* gameSound, const DeverbReport& report, const audio::PcmLock& lock, const char* path) {
			const WAVEFORMATEX& format = lock.format;
			const double peakTime = FramesToMilliseconds(report.peakFrame, format);

			switch (report.result) {
			case DeverbResult::Applied:
				SEA_LOG("[Gunshot] Deverbed %p: %lu Hz, %u ch, %.0f ms -> %.0f ms, Peak %.1f dBFS at %.0f ms, "
						"Fade at %.0f ms, Path=\"%.200s\"",
					gameSound, format.nSamplesPerSec, format.nChannels, FramesToMilliseconds(lock.FrameCount(), format),
					FramesToMilliseconds(report.endFrame, format), report.peakLevel, peakTime,
					FramesToMilliseconds(report.fadeStartFrame, format), path);
				break;

			case DeverbResult::Silent:
				SEA_LOG("[Gunshot] Rejected %p: Silent, Path=\"%.200s\"", gameSound, path);
				break;

			case DeverbResult::LatePeak:
				SEA_LOG("[Gunshot] Rejected %p: Peak at %.0f ms of %.0f ms is past the keep fraction, Path=\"%.200s\"",
					gameSound, peakTime, FramesToMilliseconds(lock.FrameCount(), format), path);
				break;

			case DeverbResult::RepeatedPeak:
				SEA_LOG("[Gunshot] Rejected %p: Repeated peak at %.0f ms (%.1f dB, first at %.0f ms), Path=\"%.200s\"",
					gameSound, FramesToMilliseconds(report.repeatFrame, format), report.repeatLevel, peakTime, path);
				break;
			}
		}

	}

	// Removes the baked-in reverb tail of a single gunshot in place, before the original `Play`.
	// A gunshot is found by its file path. Looping sounds and sounds with more than one shot are left as they are.
	// Logs each deverbed and rejected buffer once with `[Debug] bLogGunshots`.
	// DSOAL only: the reverb replaces the removed tail.
	//
	// Thread: Audio
	void ProcessGunshotFilter(void* gameSound, bool loop) {
		const config::Settings& settings = config::Get();

		if (!settings.gunshots.deverb || !settings.reverb.enabled || reverb::IsBypassed() || !audio::IsDSOALLoaded()) {
			return;
		}

		if (loop || (Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags) & kExcludedFlags)) {
			return;
		}

		const char* path = &Field<char>(gameSound, engine::kSound_FilePath);

		if (engine::ClassifyGunfirePath(path) != engine::GunfireKind::Shot) {
			return;
		}

		audio::PcmLock lock;
		const audio::PcmLockResult lockResult = audio::LockPcmBuffer(gameSound, lock);

		if (lockResult == audio::PcmLockResult::UnsupportedFormat && settings.debug.logGunshots) {
			LogUnsupportedFormat(lock.format, path);
		}

		if (lockResult != audio::PcmLockResult::Locked) {
			return;
		}

		const std::uint64_t contentHash = hash::Fnv1a(lock.audio, lock.audioBytes);

		if (IsHandled(contentHash)) {
			audio::UnlockPcmBuffer(lock);

			return;
		}

		const DeverbReport report = ApplyDeverb(lock.Samples(), lock.FrameCount(), lock.format.nChannels,
			static_cast<double>(lock.format.nSamplesPerSec), ParametersFromSettings(settings.gunshots));

		if (report.result == DeverbResult::Applied) {
			RememberHandled(hash::Fnv1a(lock.audio, lock.audioBytes));
		} else {
			RememberHandled(contentHash);
		}

		audio::UnlockPcmBuffer(lock);

		if (settings.debug.logGunshots) {
			LogReport(gameSound, report, lock, path);
		}
	}

}
