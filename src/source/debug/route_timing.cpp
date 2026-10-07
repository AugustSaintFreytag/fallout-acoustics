#include "debug/route_timing.h"

#include "config/settings.h"
#include "utils/log.h"

#include <Windows.h>

#include <algorithm>
#include <mutex>

namespace sea::debug {
	namespace {
		constexpr std::uint32_t kSummaryInterval = 500;

		std::mutex g_lock;
		std::uint32_t g_sampleCount = 0;
		std::int64_t g_totalTicks = 0;
		std::int64_t g_maxTicks = 0;

		std::int64_t Now() {
			LARGE_INTEGER counter{};
			QueryPerformanceCounter(&counter);

			return counter.QuadPart;
		}

		double TicksToMicroseconds(double ticks) {
			static const std::int64_t frequency = [] {
				LARGE_INTEGER value{};
				QueryPerformanceFrequency(&value);

				return value.QuadPart;
			}();

			return ticks * 1'000'000.0 / static_cast<double>(frequency);
		}
	}

	std::int64_t BeginRouteTiming() {
		if (!config::Get().debug.logRouteTiming) {
			return 0;
		}

		return Now();
	}

	void EndRouteTiming(std::int64_t startTime) {
		if (startTime == 0) {
			return;
		}

		const std::int64_t elapsed = Now() - startTime;

		std::lock_guard guard(g_lock);

		++g_sampleCount;
		g_totalTicks += elapsed;
		g_maxTicks = std::max(g_maxTicks, elapsed);

		if (g_sampleCount < kSummaryInterval) {
			return;
		}

		const double averageMicroseconds = TicksToMicroseconds(static_cast<double>(g_totalTicks) / g_sampleCount);
		const double maxMicroseconds = TicksToMicroseconds(static_cast<double>(g_maxTicks));

		SEA_LOG("[Timing] Route: %u sounds, Avg %.1f us, Max %.1f us (DeferEaxSets=%d)", g_sampleCount,
			averageMicroseconds, maxMicroseconds, config::Get().debug.deferEaxSets);

		g_sampleCount = 0;
		g_totalTicks = 0;
		g_maxTicks = 0;
	}
}
