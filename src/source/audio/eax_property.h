#pragma once

#include "audio/directsound.h"

namespace sea::eax {
	// Result is limited to the EAX level range [kMinLevel, 0].
	LONG DecibelsToMillibels(float decibels);

	// Audio thread. Logs the first 10 failures.
	bool SetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, const void* data, ULONG size,
		const char* description);

	// Audio thread. Logs each failure.
	bool GetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, void* data, ULONG size);
}
