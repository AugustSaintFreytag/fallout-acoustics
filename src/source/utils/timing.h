#pragma once

#include <Windows.h>

#include <cstdint>

namespace sea::timing {

	// Returns the current `QueryPerformanceCounter` ticks.
	inline std::int64_t Now() {
		LARGE_INTEGER counter{};
		QueryPerformanceCounter(&counter);

		return counter.QuadPart;
	}

	// Converts game ticks to microseconds.
	inline double TicksToMicroseconds(double ticks) {
		static const std::int64_t frequency = [] {
			LARGE_INTEGER value{};
			QueryPerformanceFrequency(&value);

			return value.QuadPart;
		}();

		return ticks * 1'000'000.0 / static_cast<double>(frequency);
	}

	// Converts game ticks to milliseconds.
	inline double TicksToMilliseconds(double ticks) {
		return TicksToMicroseconds(ticks) / 1000.0;
	}

}
