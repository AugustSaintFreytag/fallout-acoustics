#pragma once
// Engine addresses and object layouts for FalloutNV.exe 1.4.0.525.
// Each value has a tag that shows its source:
//   JIP  = reference/JIP-LN-NVSE
//   JG   = reference/JohnnyGuitarNVSE
//   ST   = reference/Stewie Tweaks 10.00 Source
//   EXE  = verified in the disassembly of re/bin/FalloutNV_1.4.0.525_unpacked.exe

#include <cstddef>
#include <cstdint>

namespace sea::game {

	// Globals (GLOB)
	constexpr std::uintptr_t kPlayerSingleton = 0x11DEA3C;   // PlayerCharacter**  (JIP)
	constexpr std::uintptr_t kCurrentAcousticSpace = 0x11DCFB4;   // BGSAcousticSpace* (JG FalloutAudio::pCurrentSpace, JIP fix hook)
	constexpr std::uintptr_t kCurrentCellAcousticSpace = 0x11DCFB8;   // BGSAcousticSpace* (JG FalloutAudio::pCurrentCellSpace)

	// TESForm (Form)
	constexpr std::uintptr_t kForm_TypeID = 0x04;  // UInt8
	constexpr std::uintptr_t kForm_RefID = 0x0C;
	constexpr std::uintptr_t kFormVtbl_GetEditorID = 0x130;  // const char* (__thiscall*)() (JIP)

	constexpr std::uint8_t kFormType_TESSound = 0x0D;
	constexpr std::uint8_t kFormType_BGSAcousticSpace = 0x0E;
	constexpr std::uint8_t kFormType_TESObjectCELL = 0x39;

	// TESObjectREFR (REFR)
	constexpr std::uintptr_t kRefr_ParentCell = 0x40;  // (JIP)

	// TESObjectCELL (CELL)
	constexpr std::uintptr_t kCell_FullNameData = 0x1C;  // TESFullName@18 -> String.m_data (JIP)
	constexpr std::uintptr_t kCell_Flags = 0x24;  // UInt8, bit 0 = interior (JIP)
	constexpr std::uintptr_t kCell_ExtraDataHead = 0x2C;  // ExtraDataList@28 -> m_data (JIP)

	// BSExtraData
	constexpr std::uintptr_t kExtra_Type = 0x04;  // UInt8 (JIP)
	constexpr std::uintptr_t kExtra_Next = 0x08;
	constexpr std::uint8_t kExtraType_CellAcousticSpace = 0x81;   // (JIP)
	constexpr std::uintptr_t kExtraCellAcousticSpace_Space = 0x0C;

	// BGSAcousticSpace (ASPC)
	constexpr std::uintptr_t kAspc_IsInterior = 0x30;  // UInt8 (JIP, JG)
	constexpr std::uintptr_t kAspc_EnvironmentType = 0x4C;  // UInt32, ANAM value (JIP, JG)

	// BSGameSound / BSWin32GameSound (SOUN)
	constexpr std::uintptr_t kVtbl_BSWin32GameSound = 0x10A3BF4;  // (JIP, ST)
	constexpr std::uintptr_t kSoundVtbl_SetEnvironmentType = 0x18;  // void(UInt32), writes +0x154 (EXE: ret 4)
	constexpr std::uintptr_t kSoundVtbl_Play = 0x30;  // bool(bool loop), reads +0x198/+0x19C (EXE: ret 4)

	constexpr std::uintptr_t kSound_ID = 0x004;
	constexpr std::uintptr_t kSound_TypeFlags = 0x008;  // SoundFlag bits (JG, JIP)
	constexpr std::uintptr_t kSound_StateFlags = 0x010;
	constexpr std::uintptr_t kSound_StaticAttenuation = 0x018;  // UInt16, unit not verified (JIP: "dB * -1000")
	constexpr std::uintptr_t kSound_ReverbAttenuation = 0x01A;  // UInt16 (JG: usReverbAttenuation)
	constexpr std::uintptr_t kSound_FilePath = 0x036;  // char[]
	constexpr std::uintptr_t kSound_SourceSound = 0x134;  // TESSound*, set only when JIP's patch is active (JIP)
	constexpr std::uintptr_t kSound_EnvironmentType = 0x154;  // UInt32 (EXE: SetEnvironmentType writes it)

	// Infrastructure
	// +1CC holds the same object as +19C.
	constexpr std::uintptr_t kWin32Sound_Device = 0x198;    // IDirectSound8*
	constexpr std::uintptr_t kWin32Sound_Buffer = 0x19C;    // IDirectSoundBuffer8* (2D and 3D). Gives IKsPropertySet with DSOAL.
	constexpr std::uintptr_t kWin32Sound_Buffer3D = 0x1D4;  // IDirectSound3DBuffer*, 3D sounds only
	constexpr std::uintptr_t kWin32Sound_ProbeBegin = 0x198;  // Layout probe scans this range for DirectSound COM pointers
	constexpr std::uintptr_t kWin32Sound_ProbeEnd = 0x230;    // Object size from JIP. ST gives 0x2E0.

	enum SoundFlag : std::uint32_t {  // (JG BSGameSound::TypeFlags)
		kSound_2D = 1u << 0,
		kSound_3D = 1u << 1,
		kSound_Voice = 1u << 2,
		kSound_Footsteps = 1u << 3,
		kSound_Loop = 1u << 4,
		kSound_SystemSound = 1u << 5,
		kSound_RandomFrequency = 1u << 6,
		kSound_Battle = 1u << 7,
		kSound_OneShot = 1u << 8,
		kSound_Music = 1u << 11,
		kSound_Region = 1u << 12,
		kSound_MuteSubmerged = 1u << 13,  // JG "MAYBE_UNDERWATER", "Mute when submerged" flag
		kSound_Impact = 1u << 14,
		kSound_DontCache = 1u << 16,
		kSound_2DGunfire = 1u << 17,
		kSound_FirstPerson = 1u << 18,
		kSound_Modulated = 1u << 19,
		kSound_Radio = 1u << 20,
		kSound_IgnoreTimescale = 1u << 21,  // Seen on player foley
		kSound_EnvelopeFast = 1u << 25,
		kSound_EnvelopeSlow = 1u << 26,
		kSound_2DRadius = 1u << 27,
		kSound_AnimationDriven = 1u << 30,  // Footsteps, etc., triggered from KF files
	};

	// ANAM value = DSFX_I3DL2_ENVIRONMENT_PRESET_* + 1 (JIP BGSAcousticSpace).
	const char* EnvironmentTypeName(std::uint32_t type);

	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback); // Case insensitive

	void DescribeSoundFlags(std::uint32_t flags, char* buffer, std::size_t size);
	
	void* GetPlayer();
	void* GetParentCell(void* reference);
	void* GetCellAcousticSpace(void* cell);  // ExtraCellAcousticSpace, or null
	
	const char* GetEditorID(void* form);     // "" if not available
	std::uint32_t GetFormID(void* form);

	// Corner notification (QueueUIMessage, 0x7052F0, JIP/xNVSE).
	void ShowNotification(const char* message);

	// Returns the TESSound* sound source at +0x134. 
	// Returns null if value is not a valid `TESSound` object.
	void* GetSourceSoundChecked(void* gameSound);
}
