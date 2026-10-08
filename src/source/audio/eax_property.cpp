#include "audio/eax_property.h"

#include "audio/eax.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>

namespace sea::eax {

	namespace {

		// Thread: Audio
		int g_setFailuresLogged = 0;

	}

	// Returns the given decibel value converted to millibels.
	// Clamps to the EAX level range [kMinLevel, 0].
	LONG DecibelsToMillibels(float decibels) {
		const LONG millibels = static_cast<LONG>(std::lround(decibels * 100.0f));

		return std::clamp(millibels, kMinLevel, 0L);
	}

	// Sets an EAX property through a buffer's property set.
	// Returns true on success.
	//
	// Thread: Audio
	bool SetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, const void* data, ULONG size,
		const char* description) {
		const HRESULT result = propertySet->Set(propertySetId, propertyId, nullptr, 0, const_cast<void*>(data), size);

		if (FAILED(result) && g_setFailuresLogged < 10) {
			++g_setFailuresLogged;
			SEA_LOG("[EAX] Could not set property: %s (HRESULT %08lX).", description, static_cast<unsigned long>(result));
		}

		return SUCCEEDED(result);
	}

	// Reads an EAX property into given data. Returns true on success.
	//
	// Thread: Audio
	bool GetProperty(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, void* data, ULONG size) {
		ULONG bytesReturned = 0;
		const HRESULT result = propertySet->Get(propertySetId, propertyId, nullptr, 0, data, size, &bytesReturned);

		if (FAILED(result)) {
			SEA_LOG("[EAX] Could not get property %08lX (HRESULT %08lX).", propertyId, static_cast<unsigned long>(result));
		}

		return SUCCEEDED(result);
	}

}
