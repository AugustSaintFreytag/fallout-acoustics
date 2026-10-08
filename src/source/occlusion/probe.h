#pragma once

#include "utils/vector3.h"

#include <cstddef>

namespace sea::occlusion {

	// Most casts one probe can use for its forward and back pass with recasts past hits.
	constexpr int kMaxCastsPerProbe = 14;

	struct ProbeResult {
		int occluders = 0;  // Distinct objects between listener and sound (terrain counts once)
		int casts = 0;
		char description[192] = "";  // Editor IDs of the first occluders, for the log
	};

	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source);

}
