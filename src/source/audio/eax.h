#pragma once

#include <Windows.h>
#include <cstdint>

// EAX 4.0 definitions, copied from OpenAL Soft (al/eax/api.h).

namespace sea::eax {

	// DSOAL forwards only EAX 1-4 property sets through IKsPropertySet.
	// Plugin uses EAX 4.0 with 2 active FX slots per source.
	// Slots 0 (reverb) and 1 (chorus) are locked legacy slots. Slots 2 and 3 can load an effect.

	inline constexpr GUID kFXSlot0 = {0xC4D79F1E, 0xF1AC, 0x436B, {0xA8, 0x1D, 0xA7, 0x38, 0xE7, 0x04, 0x54, 0x69}};
	inline constexpr GUID kFXSlot2 = {0x1D433B88, 0xF0F6, 0x4637, {0x91, 0x9F, 0x60, 0xE7, 0xE0, 0x6B, 0x5E, 0xDD}};
	inline constexpr GUID kReverbEffect = {0x0CF95C8F, 0xA3CC, 0x4849, {0xB0, 0xB6, 0x83, 0x2E, 0xCC, 0x18, 0x22, 0xDF}};
	inline constexpr GUID kSource = {0x1B86B823, 0x22DF, 0x4EAE, {0x8B, 0x3C, 0x12, 0x78, 0xCE, 0x54, 0x42, 0x27}};
	inline constexpr GUID kNull = {};

	// Property ID flag. The value is stored and applied at the next immediate set.
	// OpenAL Soft ignores the flag on FX slot properties (kFXSlot_*). These are always immediate.
	inline constexpr ULONG kDeferred = 0x80000000;

	// FX Slot Properties
	// Property set: kFXSlotN
	enum : ULONG {
		kFXSlot_LoadEffect = 0x10002,  // GUID of the effect. Resets the effect properties.
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

	// Reverb property ranges. Reference at "al/eax/api.h", "EAXREVERB_MIN*" and "EAXREVERB_MAX*".
	inline constexpr LONG kMaxReflections = 1000;  // mB
	inline constexpr LONG kMaxReverb = 2000;  // mB
	inline constexpr float kMinDecayTime = 0.1f;  // Seconds
	inline constexpr float kMaxDecayTime = 20.0f;  // Seconds
	inline constexpr float kMinDecayHFRatio = 0.1f;
	inline constexpr float kMaxDecayHFRatio = 2.0f;
	inline constexpr float kMaxReflectionsDelay = 0.3f;  // Seconds

	struct Vector {
		float x, y, z;

		bool operator==(const Vector&) const = default;
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

		bool operator==(const ReverbProperties&) const = default;
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
