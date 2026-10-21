#pragma once

#include "utils/vector3.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sea::occlusion {

	// Most casts one probe can use for its forward and back pass with recasts past hits.
	constexpr int kMaxCastsPerProbe = 14;

	// A reference or terrain hit between listener and sound.
	// Taken from nearest to furthest hit along the ray.
	struct Occluder {
		void* reference = nullptr;  // Null for terrain
		std::uint8_t collisionLayer = 0;  // Of the first hit
		float nearDistance = 0.0f;  // Game units from listener
		float farDistance = 0.0f;  // Game units from listener
		float weight = 1.0f;  // Share of `fWallLevel`
	};

	// A logical physical layer between listener and sound.
	// Occluders in close proximity get grouped together and counted as one.
	struct OcclusionLayer {
		std::vector<Occluder> occluders;
		float nearDistance = 0.0f;  // Game units from listener
		float farDistance = 0.0f;  // Game units from listener
		float weight = 0.0f;  // Share of `fWallLevel`
	};

	// The result of one occlusion probe between listener and sound.
	struct ProbeResult {
		std::vector<OcclusionLayer> layers;  // Sorted by distance from listener, terrain included
		void* emitter = nullptr;  // Reference with its origin at the sound, null if none was hit
		bool emitterExcluded = false;  // Emitter is in `[OcclusionEmitterBlacklist]`, no layers probed
		float weight = 0.0f;  // Sum of layer weights, 1 for a wall
		int casts = 0;
	};

	float OccluderSize(void* reference);

	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source);

}
