#pragma once

#include <Windows.h>
#include <cstdint>

// EAX 4.0 definitions, copied from OpenAL Soft (al/eax/api.h).

namespace sea::eax {

	// DSOAL forwards only EAX 1-4 property sets through IKsPropertySet.
	// Plugin uses EAX 4.0 with 2 active FX slots per source.

	inline constexpr GUID kFXSlot0 = {0xC4D79F1E, 0xF1AC, 0x436B, {0xA8, 0x1D, 0xA7, 0x38, 0xE7, 0x04, 0x54, 0x69}};
	inline constexpr GUID kSource = {0x1B86B823, 0x22DF, 0x4EAE, {0x8B, 0x3C, 0x12, 0x78, 0xCE, 0x54, 0x42, 0x27}};
	inline constexpr GUID kNull = {};

	// Property ID flag. The value is stored and applied at the next immediate set.
	// OpenAL Soft ignores the flag on FX slot properties (kFXSlot_*). These are always immediate.
	inline constexpr ULONG kDeferred = 0x80000000;

	// FX Slot Properties
	// Property set: kFXSlotN
	enum : ULONG {
		kFXSlot_Volume = 0x10003,  // LONG, mB [-10000, 0]
	};

	// Reverb Effect Properties
	// Property set: kFXSlotN (with a reverb loaded)
	enum : ULONG {
		kReverb_AllParameters = 1,  // ReverbProperties
	};

	// Source Properties
	// Property set: kSource (on each buffer)
	enum : ULONG {
		kSource_ObstructionParameters = 2,  // ObstructionProperties
		kSource_OcclusionParameters = 3,  // OcclusionProperties
		kSource_Direct = 5,  // LONG, mB, [-10000, 1000]
		kSource_SendParameters = 23,  // SourceSendProperties[]
		kSource_ActiveFXSlotID = 27,  // ActiveFXSlots
	};

	// Default ratios of source occlusion.
	//
	// OpenAL applies source occlusion to the direct path only if the source has the primary FX slot (slot 0) active.
	// Reference at "al/source.cpp", "eax_create_direct_filter_param".
	inline constexpr float kDefaultOcclusionLFRatio = 0.25f;
	inline constexpr float kDefaultOcclusionRoomRatio = 1.5f;
	inline constexpr float kDefaultOcclusionDirectRatio = 1.0f;

	inline constexpr LONG kMinLevel = -10000;  // mB, silence
	inline constexpr LONG kMaxDirect = 1000;  // mB

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

	struct ObstructionProperties {  // EAXOBSTRUCTIONPROPERTIES
		LONG obstruction;  // mB, high frequencies of direct path
		float obstructionLFRatio;
	};

	struct OcclusionProperties {  // EAXOCCLUSIONPROPERTIES
		LONG occlusion;  // mB, high frequencies of direct path and reverb out
		float occlusionLFRatio;
		float occlusionRoomRatio;
		float occlusionDirectRatio;
	};

	static_assert(sizeof(ObstructionProperties) == 8);
	static_assert(sizeof(OcclusionProperties) == 16);

}
