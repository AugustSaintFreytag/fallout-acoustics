#include "debug/request_probe.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/threads.h"
#include "utils/timing.h"

#include <Windows.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sea::debug {

	using mem::Field;

	namespace {

		constexpr std::uint32_t kInvalidSoundId = 0xFFFFFFFF;
		constexpr std::uint32_t kSummaryInterval = 300;
		constexpr double kSummaryIntervalMs = 30000.0;
		constexpr std::size_t kMaxPendingRequests = 4096;
		constexpr std::size_t kTopCallerCount = 8;

		struct PendingRequest {
			std::int64_t playTime = 0;  // 0 = no play request yet
			std::uint32_t threadId = 0;
			bool mainThread = false;
			bool positionBeforePlay = false;
			bool followsObject = false;
			SoundRequest kind = SoundRequest::Play;
			std::uintptr_t caller = 0;  // Return address of the play request
		};

		struct RouteStats {
			std::uint32_t started = 0;
			std::uint32_t mainThread = 0;
			std::uint32_t otherThread = 0;
			std::uint32_t unmatched = 0;  // Started without a play request through BSSoundHandle
			std::uint32_t positionBeforePlay = 0;
			std::uint32_t followsObject = 0;
			std::uint32_t delayed = 0;  // PlayAfter or FadeInPlay
			double totalLatencyMs = 0.0;
			double maxLatencyMs = 0.0;
		};

		// Thread: Any (under `g_lock`)
		std::mutex g_lock;
		std::unordered_map<std::uint32_t, PendingRequest> g_pending;
		std::map<const char*, RouteStats> g_routeStats;  // Route labels are string literals
		std::unordered_map<std::uintptr_t, std::uint32_t> g_otherThreadCallers;
		std::unordered_map<std::uint32_t, std::uint32_t> g_requestThreads;  // Thread ID -> play requests
		std::uint32_t g_startedCount = 0;
		std::int64_t g_lastSummaryTime = 0;

		bool IsPlayRequest(SoundRequest request) {
			return request == SoundRequest::Play || request == SoundRequest::PlayAfter || request == SoundRequest::FadeInPlay;
		}

		// Logs return addresses with the most sound requests from threads other than main.
		void LogTopCallers() {
			std::vector<std::pair<std::uintptr_t, std::uint32_t>> callers(g_otherThreadCallers.begin(), g_otherThreadCallers.end());

			std::sort(callers.begin(), callers.end(), [](const auto& left, const auto& right) {
				return left.second > right.second;
			});

			if (callers.size() > kTopCallerCount) {
				callers.resize(kTopCallerCount);
			}

			for (const auto& [caller, count] : callers) {
				SEA_LOG("[Requests]   Caller from other thread: return address %08X, %u requests", static_cast<unsigned>(caller), count);
			}
		}

		// Logs which code requested a started sound. Comes before the sound's [Play] line.
		void LogRequest(std::uint32_t soundId, const PendingRequest& pending, double latencyMs) {
			char modulePath[MAX_PATH] = "";
			const char* moduleName = mem::ModuleNameAt(pending.caller, modulePath, sizeof(modulePath));
			const char* threadName = "other";

			if (pending.mainThread) {
				threadName = "main";
			}

			SEA_LOG("[Request] ID=%u from %s thread %u, caller %08X (%s), position first %d, follows object %d, %.1f ms",
				soundId, threadName, pending.threadId, static_cast<unsigned>(pending.caller), moduleName,
				pending.positionBeforePlay, pending.followsObject, latencyMs);
		}

		void LogSummary() {
			SEA_LOG("[Requests] %u sounds started. Main thread %u, audio thread %u.", g_startedCount, threads::MainThreadId(),
				threads::AudioThreadId());

			for (const auto& [threadId, count] : g_requestThreads) {
				const char* threadName = "other";

				if (threadId == threads::MainThreadId()) {
					threadName = "main";
				} else if (threadId == threads::AudioThreadId()) {
					threadName = "audio";
				}

				SEA_LOG("[Requests]   Thread %u (%s): %u play requests", threadId, threadName, count);
			}

			for (const auto& [route, stats] : g_routeStats) {
				const std::uint32_t matched = stats.mainThread + stats.otherThread;
				double averageLatencyMs = 0.0;

				if (matched > 0) {
					averageLatencyMs = stats.totalLatencyMs / matched;
				}

				SEA_LOG("[Requests]   %-10s %5u started: main %u, other %u, unmatched %u, delayed %u, position first %u, "
						"follows object %u, latency avg %.1f ms, max %.1f ms",
					route, stats.started, stats.mainThread, stats.otherThread, stats.unmatched, stats.delayed,
					stats.positionBeforePlay, stats.followsObject, averageLatencyMs, stats.maxLatencyMs);
			}

			LogTopCallers();
		}

	}

	// Records thread and caller of a sound's play request. Also records position or follow requests before it.
	// Caller is the return address into the code that called the handle function.
	// Called from `BSSoundHandle` hooks on the requesting thread.
	//
	// Thread: Any
	void OnSoundRequested(std::uint32_t soundId, SoundRequest request, std::uintptr_t caller) {
		if (soundId == kInvalidSoundId) {
			return;
		}

		const std::uint32_t threadId = GetCurrentThreadId();
		const bool mainThread = threads::IsMainThread();
		const std::int64_t now = timing::Now();

		std::lock_guard guard(g_lock);

		// Requests of sounds that never start stay in the table. Clear it when full.
		if (g_pending.size() >= kMaxPendingRequests) {
			g_pending.clear();
		}

		PendingRequest& pending = g_pending[soundId];

		if (request == SoundRequest::SetPosition) {
			if (pending.playTime == 0) {
				pending.positionBeforePlay = true;
			}

			return;
		}

		if (request == SoundRequest::SetObjectToFollow) {
			pending.followsObject = true;

			return;
		}

		// Only the first play request of a sound counts.
		if (!IsPlayRequest(request) || pending.playTime != 0) {
			return;
		}

		pending.playTime = now;
		pending.threadId = threadId;
		pending.mainThread = mainThread;
		pending.kind = request;
		pending.caller = caller;

		++g_requestThreads[threadId];

		if (!mainThread) {
			++g_otherThreadCallers[caller];
		}
	}

	// Matches a started sound with its request and adds it to its route's statistics.
	// Logs a summary every 300 sounds or 30 seconds on the next started sound.
	// Called from `BSWin32GameSound::Play` with the sound's route.
	//
	// Thread: Audio
	void OnSoundStarted(void* sound, const char* route) {
		if (!config::Get().debug.probeSoundRequests) {
			return;
		}

		const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);
		const std::int64_t now = timing::Now();

		std::lock_guard guard(g_lock);

		RouteStats& stats = g_routeStats[route];
		++stats.started;
		++g_startedCount;

		const auto entry = g_pending.find(soundId);

		if (entry == g_pending.end() || entry->second.playTime == 0) {
			++stats.unmatched;
		} else {
			const PendingRequest& pending = entry->second;
			const double latencyMs = timing::TicksToMilliseconds(static_cast<double>(now - pending.playTime));

			if (pending.mainThread) {
				++stats.mainThread;
			} else {
				++stats.otherThread;
			}

			if (pending.positionBeforePlay) {
				++stats.positionBeforePlay;
			}

			if (pending.followsObject) {
				++stats.followsObject;
			}

			if (pending.kind != SoundRequest::Play) {
				++stats.delayed;
			}

			stats.totalLatencyMs += latencyMs;
			stats.maxLatencyMs = std::max(stats.maxLatencyMs, latencyMs);

			if (config::Get().debug.logSoundPlay) {
				LogRequest(soundId, pending, latencyMs);
			}
		}

		if (entry != g_pending.end()) {
			g_pending.erase(entry);
		}

		if (g_lastSummaryTime == 0) {
			g_lastSummaryTime = now;
		}

		const double msSinceSummary = timing::TicksToMilliseconds(static_cast<double>(now - g_lastSummaryTime));

		if (g_startedCount % kSummaryInterval == 0 || msSinceSummary >= kSummaryIntervalMs) {
			LogSummary();
			g_lastSummaryTime = now;
		}
	}

}
