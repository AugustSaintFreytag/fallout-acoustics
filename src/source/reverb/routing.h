#pragma once

#include <cstdint>

namespace sea::reverb {

	// Properties for a sound routing.
	struct Route {
		const char* label;
		float sendLevel;
	};

	// Selects the send category from the SoundFlag bits of a sound.
	Route Classify(std::uint32_t soundFlags);

}
