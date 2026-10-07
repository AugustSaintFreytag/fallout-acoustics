#pragma once

#include <cstdint>

namespace sea::reverb {
	struct Route {
		const char* label;
		float sendDb;
	};

	// Selects the send category from the SoundFlag bits of a sound.
	Route Classify(std::uint32_t soundFlags);
}
