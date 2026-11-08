#pragma once

#include <optional>

namespace sea::reverb {

	std::optional<float> OnThresholdShiftPlay(void* gameSound, float sourceLevel);

	void OnThresholdShiftUpdate(void* gameSound);

}
