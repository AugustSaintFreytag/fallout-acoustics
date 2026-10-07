#pragma once

#include "audio/directsound.h"

namespace sea::debug {
	// Audio thread. Logs what OpenAL Soft holds for FX slot 0, if `[Debug] iReadbackCount` is set.
	void ReadBackSlot(IKsPropertySet* propertySet);

	// Audio thread. Logs the source state of the first `iReadbackCount` routed sounds.
	void ReadBackSource(IKsPropertySet* propertySet, const char* label);
}
