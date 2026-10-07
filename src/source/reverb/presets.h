#pragma once

#include "audio/eax.h"

#include <cstdint>
#include <iterator>

namespace sea::reverb {
	// Index = ANAM - 1.
	inline constexpr eax::ReverbProperties kPresets[] = {
#include "reverb/presets.inc"
	};

	inline constexpr std::uint32_t kEnvironmentCount = static_cast<std::uint32_t>(std::size(kPresets));  // ANAM 1..30
	static_assert(kEnvironmentCount == 30);
}
