#include "debug/pick_census.h"

#include "config/settings.h"
#include "utils/log.h"
#include "utils/threads.h"
#include "utils/timing.h"

#include <Windows.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace sea::debug {

	namespace {

		constexpr double kSummaryIntervalMs = 30000.0;
		constexpr std::size_t kTopCallersPerThread = 5;

		struct ThreadStats {
			std::uint32_t picks = 0;
			std::map<std::uintptr_t, std::uint32_t> callers;  // Return address -> picks
		};

		// Thread: Any (under `g_lock`)
		std::mutex g_lock;
		std::map<std::uint32_t, ThreadStats> g_threads;  // Thread ID -> stats since start

		// Thread: Main
		std::int64_t g_lastSummaryTime = 0;

		const char* ThreadName(std::uint32_t threadId) {
			if (threadId == threads::MainThreadId()) {
				return "main";
			}

			if (threadId == threads::AudioThreadId()) {
				return "audio";
			}

			return "other";
		}

		// Logs return addresses with the most picks on one thread.
		void LogTopCallers(const ThreadStats& stats) {
			std::vector<std::pair<std::uintptr_t, std::uint32_t>> callers(stats.callers.begin(), stats.callers.end());

			std::sort(callers.begin(), callers.end(), [](const auto& left, const auto& right) {
				return left.second > right.second;
			});

			if (callers.size() > kTopCallersPerThread) {
				callers.resize(kTopCallersPerThread);
			}

			for (const auto& [caller, count] : callers) {
				SEA_LOG("[Picks]     return address %08X: %u picks", static_cast<unsigned>(caller), count);
			}
		}

		void LogSummary() {
			std::lock_guard guard(g_lock);

			SEA_LOG("[Picks] Engine picks since start, per thread (main %u):", threads::MainThreadId());

			for (const auto& [threadId, stats] : g_threads) {
				SEA_LOG("[Picks]   Thread %u (%s): %u picks", threadId, ThreadName(threadId), stats.picks);
				LogTopCallers(stats);
			}
		}

	}

	// Counts an engine pick by thread and caller. Caller is the return address into the code that made the pick.
	// Called from `bhkWorld::PickObject` hooks. Not called for the plugin's own casts.
	//
	// Thread: Any
	void OnEnginePick(std::uintptr_t caller) {
		const std::uint32_t threadId = GetCurrentThreadId();

		std::lock_guard guard(g_lock);

		ThreadStats& stats = g_threads[threadId];
		++stats.picks;
		++stats.callers[caller];
	}

	// Logs a summary every 30 seconds: picks per thread and top callers on each.
	//
	// Thread: Main (each frame)
	void PollPickCensus() {
		if (!config::Get().debug.probePickThreads) {
			return;
		}

		const std::int64_t now = timing::Now();

		if (g_lastSummaryTime == 0) {
			g_lastSummaryTime = now;

			return;
		}

		if (timing::TicksToMilliseconds(static_cast<double>(now - g_lastSummaryTime)) < kSummaryIntervalMs) {
			return;
		}

		g_lastSummaryTime = now;
		LogSummary();
	}

}
