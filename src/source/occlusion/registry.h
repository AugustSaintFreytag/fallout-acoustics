#pragma once

#include "utils/vector3.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sea::occlusion {

	struct LiveSound {
		std::uint32_t soundId = 0;
		Vector3 position;
	};

	void PublishSound(std::uint32_t soundId, const Vector3& position, const char* path);

	bool GetTarget(std::uint32_t soundId, std::int32_t& occlusionMb);

	std::vector<LiveSound> CollectSounds();

	void SetTarget(std::uint32_t soundId, std::int32_t occlusionMb);

	std::string GetPath(std::uint32_t soundId);

}
