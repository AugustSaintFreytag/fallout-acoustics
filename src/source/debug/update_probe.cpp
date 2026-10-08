#include "debug/update_probe.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/objects.h"
#include "engine/sound_flags.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/threads.h"
#include "utils/timing.h"
#include "utils/vector3.h"

#include <Windows.h>

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace sea::debug {

	using mem::Field;

	namespace {

		constexpr double kSummaryIntervalMs = 5000.0;
		constexpr std::size_t kMaxTrackedSounds = 1024;
		constexpr std::size_t kSamplesPerSummary = 3;

		struct IntervalStats {
			std::uint32_t count = 0;
			double totalMs = 0.0;
			double maxMs = 0.0;
		};

		// Thread: Any (under `g_lock`)
		std::mutex g_lock;
		std::int64_t g_summaryStartTime = 0;
		std::uint32_t g_callCount = 0;
		std::unordered_map<std::uint32_t, std::uint32_t> g_callsPerThread;
		std::unordered_map<std::uint32_t, std::int64_t> g_lastUpdateTime;  // Sound ID -> time
		std::unordered_set<std::uint32_t> g_soundsInSummary;
		IntervalStats g_intervals;
		std::size_t g_samplesLogged = 0;

		const char* ThreadName(std::uint32_t threadId) {
			if (threadId == threads::AudioThreadId()) {
				return "audio";
			}

			if (threadId == threads::MainThreadId()) {
				return "main";
			}

			return "other";
		}

		// Logs a sound's emitter position next to the player position.
		void LogSample(void* sound, std::uint32_t soundId, std::uint32_t flags) {
			const Vector3 emitter = Field<Vector3>(sound, engine::kWin32Sound_EmitterPosition);
			const Vector3 player = engine::GetPosition(engine::GetPlayer());

			char flagText[160];
			engine::DescribeSoundFlags(flags, flagText, sizeof(flagText));

			SEA_LOG("[Update]   Sample ID=%u [%s] Emitter (%.1f, %.1f, %.1f), player (%.1f, %.1f, %.1f), distance %.1f, "
					"Path=\"%.120s\"",
				soundId, flagText, emitter.x, emitter.y, emitter.z, player.x, player.y, player.z, Distance(emitter, player),
				&Field<char>(sound, engine::kSound_FilePath));
		}

		void LogSummary(double elapsedMs) {
			double averageIntervalMs = 0.0;

			if (g_intervals.count > 0) {
				averageIntervalMs = g_intervals.totalMs / g_intervals.count;
			}

			SEA_LOG("[Update] %.1f s: %u calls for %zu sounds, interval per sound avg %.1f ms, max %.1f ms", elapsedMs / 1000.0,
				g_callCount, g_soundsInSummary.size(), averageIntervalMs, g_intervals.maxMs);

			for (const auto& [threadId, count] : g_callsPerThread) {
				SEA_LOG("[Update]   Thread %u (%s): %u calls", threadId, ThreadName(threadId), count);
			}
		}

		void ResetSummary(std::int64_t now) {
			g_summaryStartTime = now;
			g_callCount = 0;
			g_callsPerThread.clear();
			g_soundsInSummary.clear();
			g_intervals = {};
			g_samplesLogged = 0;
		}

	}

	// Counts calls per thread and update intervals per sound. Logs a few 3D emitter positions.
	// Logs a summary every 5 seconds. Called from `BSWin32GameSound::Update`.
	//
	// Thread: Any
	void OnSoundUpdate(void* sound) {
		if (!config::Get().debug.probeSoundUpdate) {
			return;
		}

		const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);
		const std::uint32_t flags = Field<std::uint32_t>(sound, engine::kSound_TypeFlags);
		const std::int64_t now = timing::Now();

		std::lock_guard guard(g_lock);

		if (g_summaryStartTime == 0) {
			ResetSummary(now);
		}

		++g_callCount;
		++g_callsPerThread[GetCurrentThreadId()];

		const bool firstUpdateInPeriod = g_soundsInSummary.insert(soundId).second;

		// Sample the first 3D sounds of each summary period.
		if (firstUpdateInPeriod && (flags & engine::kSound_3D) && g_samplesLogged < kSamplesPerSummary) {
			LogSample(sound, soundId, flags);
			++g_samplesLogged;
		}

		if (g_lastUpdateTime.size() >= kMaxTrackedSounds) {
			g_lastUpdateTime.clear();
		}

		const auto previous = g_lastUpdateTime.find(soundId);

		if (previous != g_lastUpdateTime.end()) {
			const double intervalMs = timing::TicksToMilliseconds(static_cast<double>(now - previous->second));

			++g_intervals.count;
			g_intervals.totalMs += intervalMs;
			g_intervals.maxMs = std::max(g_intervals.maxMs, intervalMs);
		}

		g_lastUpdateTime[soundId] = now;

		const double elapsedMs = timing::TicksToMilliseconds(static_cast<double>(now - g_summaryStartTime));

		if (elapsedMs < kSummaryIntervalMs) {
			return;
		}

		LogSummary(elapsedMs);
		ResetSummary(now);
	}

}
