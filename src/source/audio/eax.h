#pragma once

#include <Windows.h>
#include <cstdint>

// EAX 4.0 definitions, copied from OpenAL Soft (al/eax/api.h).

namespace sea::eax {
	// DSOAL forwards only the EAX 1-4 property sets through IKsPropertySet.
	// Plugin uses EAX 4.0, exposes 2 active FX slots for each source.

	inline constexpr GUID kFXSlot0 = {0xC4D79F1E, 0xF1AC, 0x436B, {0xA8, 0x1D, 0xA7, 0x38, 0xE7, 0x04, 0x54, 0x69}};
	inline constexpr GUID kSource = {0x1B86B823, 0x22DF, 0x4EAE, {0x8B, 0x3C, 0x12, 0x78, 0xCE, 0x54, 0x42, 0x27}};
	inline constexpr GUID kNull = {};

	// Property ID flag. The value is stored and applied at the next immediate set.
	// OpenAL Soft ignores the flag on FX slot properties (kFXSlot_*), these are always immediate.
	inline constexpr ULONG kDeferred = 0x80000000;

	// FX slot properties
	// Property set: kFXSlotN
	enum : ULONG {
		kFXSlot_Volume = 0x10003,  // LONG, mB [-10000, 0]
	};

	// Reverb effect properties
	// Property set: kFXSlotN (with a reverb loaded)
	enum : ULONG {
		kReverb_AllParameters = 1,  // ReverbProperties
	};

	// Source properties
	// Property set: kSource (on each buffer)
	enum : ULONG {
		kSource_SendParameters = 23,  // SourceSendProperties[]
		kSource_ActiveFXSlotID = 27,  // ActiveFXSlots
	};

	inline constexpr LONG kMinLevel = -10000;  // mB, silence

	struct Vector {
		float x, y, z;
	};

	struct ReverbProperties {  // EAXREVERBPROPERTIES
		std::uint32_t environment;
		float environmentSize;
		float environmentDiffusion;
		LONG room;
		LONG roomHF;
		LONG roomLF;
		float decayTime;
		float decayHFRatio;
		float decayLFRatio;
		LONG reflections;
		float reflectionsDelay;
		Vector reflectionsPan;
		LONG reverb;
		float reverbDelay;
		Vector reverbPan;
		float echoTime;
		float echoDepth;
		float modulationTime;
		float modulationDepth;
		float airAbsorptionHF;
		float hfReference;
		float lfReference;
		float roomRolloffFactor;
		std::uint32_t flags;
	};

	static_assert(sizeof(ReverbProperties) == 112);

	struct ActiveFXSlots {  // EAX40ACTIVEFXSLOTS
		GUID slots[2];
	};

	struct SourceSendProperties {  // EAXSOURCESENDPROPERTIES
		GUID receivingFXSlotID;
		LONG send;    // mB
		LONG sendHF;  // mB
	};

	static_assert(sizeof(SourceSendProperties) == 24);
}
