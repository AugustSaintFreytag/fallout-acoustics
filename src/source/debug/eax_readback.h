#pragma once

#include "audio/directsound.h"

namespace sea::debug {

	void ReadBackSlot(IKsPropertySet* propertySet);
	void ReadBackSource(IKsPropertySet* propertySet, const char* label);

}
