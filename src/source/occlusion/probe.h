#pragma once

#include "utils/vector3.h"

#include <cstddef>

namespace sea::occlusion {

	// Most casts one probe can use for its forward and back pass with recasts past hits.
	constexpr int kMaxCastsPerProbe = 14;

	struct ProbeResult {
		int layers = 0;  // Physical layers between listener and sound (terrain counts once)
		float weight = 0.0f;  // Sum of layer weights, 1 for a wall
		int casts = 0;
		char description[256] = "";  // Editor IDs and distances of the first layers, for the log
	};

	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source);

}
