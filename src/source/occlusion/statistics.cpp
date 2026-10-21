#include "occlusion/statistics.h"

#include "config/settings.h"
#include "utils/log.h"
#include "utils/timing.h"

#include <cstdint>

namespace sea::occlusion {

	namespace {

		constexpr double kReportIntervalMs = 5000.0;
		constexpr double kWarningIntervalMs = 30000.0;

		// Sums of frame statistics over one report interval.
		struct OcclusionStatistics {
			std::int64_t startTime = 0;
			int frames = 0;
			int depletedFrames = 0;
			std::int64_t liveSounds = 0;
			std::int64_t soundsInRange = 0;
			std::int64_t probes = 0;
			std::int64_t casts = 0;
		};

		// Thread: Main
		OcclusionStatistics g_statistics;
		std::int64_t g_lastWarningTime = 0;

		// Logs the budget load of an interval. 100% means probing every sound in range each `fRefreshInterval`
		// takes the whole `iRayBudget`. Above 100% sounds wait longer than `fRefreshInterval` for their next probe.
		// Logs with `[Debug] bLogOcclusion`, and as a warning at most every 30 seconds if the budget ran out.
		void Report(const OcclusionStatistics& statistics, double seconds, std::int64_t now) {
			const config::Settings& settings = config::Get();
			const bool logReport = settings.debug.logOcclusion;
			const bool depleted = statistics.depletedFrames > 0;
			const bool warningDue = g_lastWarningTime == 0
				|| timing::TicksToMilliseconds(static_cast<double>(now - g_lastWarningTime)) >= kWarningIntervalMs;

			if (!logReport && !(depleted && warningDue)) {
				return;
			}

			const double frames = static_cast<double>(statistics.frames);
			const double framesPerSecond = frames / seconds;
			const double liveSounds = static_cast<double>(statistics.liveSounds) / frames;
			const double soundsInRange = static_cast<double>(statistics.soundsInRange) / frames;
			const double probesPerSecond = static_cast<double>(statistics.probes) / seconds;
			const double castsPerSecond = static_cast<double>(statistics.casts) / seconds;
			const double refreshInterval = settings.occlusion.refreshInterval;

			double castsPerProbe = 0.0;

			if (statistics.probes > 0) {
				castsPerProbe = static_cast<double>(statistics.casts) / static_cast<double>(statistics.probes);
			}

			// Casts per second to probe every sound in range once per refresh interval.
			double requiredCasts = 0.0;

			if (refreshInterval > 0.0) {
				requiredCasts = soundsInRange / refreshInterval * castsPerProbe;
			}

			const double availableCasts = settings.occlusion.rayBudget * framesPerSecond;
			double load = 0.0;

			if (availableCasts > 0.0) {
				load = requiredCasts / availableCasts * 100.0;
			}

			// Average time between two probes of the same sound.
			double effectiveRefresh = 0.0;

			if (probesPerSecond > 0.0) {
				effectiveRefresh = soundsInRange / probesPerSecond;
			}

			const double depletedShare = statistics.depletedFrames / frames * 100.0;

			const char* prefix = "";

			if (depleted) {
				prefix = "Warning: Ray budget depleted. ";
				g_lastWarningTime = now;
			}

			SEA_LOG("[Occlusion] %sBudget: %.0f%% (sounds: %.0f live, %.0f in range, probes: %.0f/s at %.1f casts, "
					"casts: %.0f/s of %.0f/s, refresh: %.2fs of %.2fs, depleted: %.0f%% of frames)",
				prefix, load, liveSounds, soundsInRange, probesPerSecond, castsPerProbe, castsPerSecond, availableCasts,
				effectiveRefresh, refreshInterval, depletedShare);
		}

	}

	// Adds the counts of one occlusion update. Reports the budget load every 5 seconds.
	//
	// Thread: Main
	void RecordFrame(const FrameStatistics& frame) {
		const std::int64_t now = timing::Now();

		if (g_statistics.startTime == 0) {
			g_statistics.startTime = now;
		}

		g_statistics.frames += 1;
		g_statistics.liveSounds += frame.numberOfLiveSounds;
		g_statistics.soundsInRange += frame.numberOfSoundsInRange;
		g_statistics.probes += frame.numberOfProbes;
		g_statistics.casts += frame.numberOfCasts;

		if (frame.isBudgetDepleted) {
			g_statistics.depletedFrames += 1;
		}

		const double elapsedMs = timing::TicksToMilliseconds(static_cast<double>(now - g_statistics.startTime));

		if (elapsedMs < kReportIntervalMs) {
			return;
		}

		Report(g_statistics, elapsedMs / 1000.0, now);
		g_statistics = OcclusionStatistics{};
		g_statistics.startTime = now;
	}

}
