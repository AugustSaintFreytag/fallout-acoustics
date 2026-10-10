#pragma once

#include <cstdint>

namespace sea::reverb {

	// A sound routing into the reverb, as picked for each sound by its category.
	struct Route {
		// Name of the route for identification and logging
		const char* label;

		// Level to send audio with (in dB)
		float sendLevel;
	};

	bool IsWorldRadio(std::uint32_t soundFlags);

	Route Classify(std::uint32_t soundFlags, bool isHolotape);

	float GetSourceLevel(std::uint32_t soundFlags, const char* path);

}
