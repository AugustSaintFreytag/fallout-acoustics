#pragma once

#include <cstdint>

namespace sea::debug {
	// Returns the start time, or 0 if `[Debug] bLogRouteTiming` is off.
	std::int64_t BeginRouteTiming();

	// Records the time since `BeginRouteTiming`. Logs a summary every 500 sounds.
	void EndRouteTiming(std::int64_t startTime);
}
