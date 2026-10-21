#pragma once

#include "utils/vector3.h"

#include <cstdint>

namespace sea::debug {

	void PollBeeper();

	void UpdateBeeper(const Vector3& soundPosition, std::int32_t targetMb);

}
