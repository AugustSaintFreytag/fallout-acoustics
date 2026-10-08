#pragma once

#include <cstdint>

namespace sea::debug {

	std::int64_t BeginRouteTiming();

	void EndRouteTiming(std::int64_t startTime);

}
