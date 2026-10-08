#pragma once

#include "audio/directsound.h"

namespace sea::eax {

	LONG DecibelsToMillibels(float decibels);

	bool SetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, const void* data, ULONG size,
		const char* description);

	bool GetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, void* data, ULONG size);

}
