#pragma once

#include "audio/directsound.h"

#include <cstdint>
#include <optional>

namespace sea::reverb {

	void ApplyTailSlot(IKsPropertySet* propertySet, std::uint32_t environment);

	std::optional<float> GetTailSendLevel(void* gameSound);

}
