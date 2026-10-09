#pragma once
// Engine addresses and object layouts for FalloutNV.exe 1.4.0.525.
// Each value has a tag that shows its source:
//   JIP   = reference/JIP-LN-NVSE
//   JG    = reference/JohnnyGuitarNVSE
//   ST    = reference/Stewie Tweaks 10.00 Source
//   xNVSE = reference/xNVSE
//   EXE   = verified in the disassembly of re/bin/FalloutNV_1.4.0.525_unpacked.exe

#include <cstdint>

namespace sea::engine {

	// Globals (GLOB)
	constexpr std::uintptr_t kPlayerSingleton = 0x11DEA3C;   // PlayerCharacter**  (JIP)
	constexpr std::uintptr_t kCurrentAcousticSpace = 0x11DCFB4;   // BGSAcousticSpace* (JG FalloutAudio::pCurrentSpace, JIP fix hook)
	constexpr std::uintptr_t kCurrentCellAcousticSpace = 0x11DCFB8;   // BGSAcousticSpace* (JG FalloutAudio::pCurrentCellSpace)
	constexpr std::uintptr_t kTESSingleton = 0x11DEA10;  // TES* (JIP)
	constexpr std::uintptr_t kSceneGraphSingleton = 0x11DEB7C;  // SceneGraph* (JIP hooks.h)

	// Functions
	constexpr std::uintptr_t kQueueUIMessage = 0x7052F0;  // Corner notification (JIP, xNVSE)

	// NiAVObject* __thiscall TES::PickObject(PickData*, bool). Nearest hit only. (JIP _GetRayCastObject; EXE: ret 8)
	constexpr std::uintptr_t kTES_PickObject = 0x458440;

	// BSSoundHandle (JG). A sound ID at +0. These functions only queue a message, the audio thread does the work later.
	// Each one starts with the same 7 bytes: push ebp / mov ebp, esp / push ecx / mov [ebp-4], ecx. (EXE)

	constexpr std::uintptr_t kSoundHandle_ID = 0x00;  // UInt32, 0xFFFFFFFF = invalid
	constexpr std::uintptr_t kSoundHandle_Play = 0xAD8830;  // bool(bool loop) (EXE: ret 4)
	constexpr std::uintptr_t kSoundHandle_PlayAfter = 0xAD8870;  // bool(UInt32 delay, UInt32 flags) (EXE: ret 8)
	constexpr std::uintptr_t kSoundHandle_SetPosition = 0xAD8B60;  // bool(float x, float y, float z) (EXE: ret 0xC)
	constexpr std::uintptr_t kSoundHandle_FadeInPlay = 0xAD8D60;  // bool(UInt32 milliseconds) (EXE: ret 4)
	constexpr std::uintptr_t kSoundHandle_SetObjectToFollow = 0xAD8F20;  // void(NiAVObject*) (EXE: ret 4)

	constexpr std::uintptr_t kSoundHandle_SetVolume = 0xAD89E0;  // bool(float volume) (JG; EXE: ret 4)

	// Open/close sounds that the engine plays twice (docs history, 2026-10-08). Each one is a `call BSSoundHandle::Play`.
	// BGSOpenCloseForm::HandleActivate (0x47A560, cdecl, JG): [ebp+0xC] = action ref. 
	// The player gets a 2D System copy, the door's animation plays a positioned copy. (EXE)
	constexpr std::uintptr_t kOpenCloseSoundPlayCall = 0x47A8FE;
	
	// Container menu sound (0x75BAF0, __thiscall(containerRef, bool open), ret 8): [ebp+8] = container ref.
	// Plays a 2D System copy when the menu opens and closes; an animated container also plays positioned copies. (EXE)
	constexpr std::uintptr_t kContainerMenuSoundPlayCall = 0x75BC57;

	// BSAudioManager function that BSSoundHandle::SetPosition calls to queue the position message.
	// __thiscall(BSAudioManager*, UInt32 soundId, float x, float y, float z). Starts with push ebp / mov ebp, esp / sub esp, 0x20. (EXE: ret 0x10)
	constexpr std::uintptr_t kAudioManager_SetPosition = 0xADB970;

	// Engine functions that request sounds from threads other than the main thread (Phase 0 request probe). (EXE)
	constexpr std::uintptr_t kImpactMixer = 0x837550;  // Havok collision sounds (ST CollisionSoundExtender: "ImpactMixer")
	constexpr std::uintptr_t kActor_VoiceSoundFunction = 0x8A1BD0;  // Voice lines, through BSSoundHandle::PlayAfter (JG)

	// Actor::VoiceSoundFunction (0x8A1BD0, JG) builds the sound flags of a voice line in [ebp-0x240].
	// At kVoiceModulationCall it calls kVoiceModulationCheck (bool __thiscall, ecx = speaking actor).
	// If the result is true, it adds Modulated. (EXE)
	constexpr std::uintptr_t kVoiceModulationCall = 0x8A2831;
	constexpr std::uintptr_t kVoiceModulationCheck = 0x880910;

	// TESForm (Form)
	constexpr std::uintptr_t kForm_TypeID = 0x04;  // UInt8
	constexpr std::uintptr_t kForm_RefID = 0x0C;
	constexpr std::uintptr_t kFormVtbl_GetEditorID = 0x130;  // const char* (__thiscall*)() (JIP)

	constexpr std::uint8_t kFormType_TESSound = 0x0D;
	constexpr std::uint8_t kFormType_BGSAcousticSpace = 0x0E;
	constexpr std::uint8_t kFormType_TESObjectACTI = 0x15;
	constexpr std::uint8_t kFormType_BGSTerminal = 0x17;
	constexpr std::uint8_t kFormType_TESObjectARMO = 0x18;
	constexpr std::uint8_t kFormType_TESObjectCONT = 0x1B;
	constexpr std::uint8_t kFormType_TESObjectDOOR = 0x1C;
	constexpr std::uint8_t kFormType_TESObjectMISC = 0x1F;
	constexpr std::uint8_t kFormType_TESObjectSTAT = 0x20;
	constexpr std::uint8_t kFormType_BGSStaticCollection = 0x21;
	constexpr std::uint8_t kFormType_BGSMovableStatic = 0x22;
	constexpr std::uint8_t kFormType_TESObjectTREE = 0x25;
	constexpr std::uint8_t kFormType_TESFurniture = 0x27;
	constexpr std::uint8_t kFormType_TESNPC = 0x2A;
	constexpr std::uint8_t kFormType_TESCreature = 0x2B;
	constexpr std::uint8_t kFormType_TESObjectCELL = 0x39;
	constexpr std::uint8_t kFormType_TESObjectREFR = 0x3A;
	constexpr std::uint8_t kFormType_Character = 0x3B;
	constexpr std::uint8_t kFormType_Creature = 0x3C;

	// TESModel component in a base form. Offset by form type, from the top byte of JIP's kBaseComponentOffsets (× 4).
	constexpr std::uintptr_t kModelOffset_Static = 0x30;  // STAT, SCOL, MSTT, TREE
	constexpr std::uintptr_t kModelOffset_Activator = 0x3C;  // ACTI, TERM, DOOR, MISC, FURN
	constexpr std::uintptr_t kModelOffset_Container = 0x48;  // CONT
	constexpr std::uintptr_t kModel_PathData = 0x04;  // String.m_data, path relative to "meshes\" (JIP)

	// TESObjectREFR (REFR)
	constexpr std::uintptr_t kRefr_BaseForm = 0x20;  // TESForm* (JIP)
	constexpr std::uintptr_t kRefr_Rotation = 0x24;  // NiVector3, radians (JIP)
	constexpr std::uintptr_t kRefr_Position = 0x30;  // NiVector3 (JIP)
	constexpr std::uintptr_t kRefr_ParentCell = 0x40;  // (JIP)
	constexpr std::uintptr_t kRefr_RenderState = 0x64;  // RenderState*, null without loaded 3D (JIP)
	constexpr std::uintptr_t kRenderState_RootNode = 0x14;  // NiNode* (JIP)

	// Actor (ACHR, ACRE)
	constexpr std::uintptr_t kActor_BaseProcess = 0x68;  // BaseProcess* (JIP)

	// The ray of JIP's _GetRayCastObject takes the collision group of the player, so that it ignores the player.
	// Pointer chain: player +0x68 (BaseProcess) +0x138 (bhkCharacterController) +0x594 +0x08 -> hkpWorldObject,
	// collision filter info (UInt32: layer, flags, group) at +0x2C. (JIP)
	constexpr std::uintptr_t kProcess_CharController = 0x138;  // (JIP HighProcess::charCtrl)
	constexpr std::uintptr_t kCharController_Unknown594 = 0x594;  // (JIP, unnamed)
	constexpr std::uintptr_t kHavokRef_Object = 0x08;  // bhkRefObject -> hkReferencedObject (JIP)
	constexpr std::uintptr_t kWorldObject_FilterInfo = 0x2C;  // hkpWorldObject: layer +0x2C, flags +0x2D, group +0x2E (JIP)

	// Character (ACHR)
	constexpr std::uintptr_t kCharacter_BipedAnim = 0x1B4;  // BipedAnim*, null without loaded 3D (JIP)

	// BipedAnim: the models that a character shows, one entry per biped slot.
	constexpr std::uintptr_t kBipedAnim_SlotData = 0x02C;  // Data[20], item (TESForm*) at +0 (JIP)
	constexpr std::uintptr_t kBipedAnim_SlotSize = 0x10;
	constexpr std::uint32_t kBipedAnim_SlotCount = 20;

	// TESObjectARMO (ARMO), TESBipedModelForm at +0x70
	constexpr std::uintptr_t kArmor_SlotMask = 0x74;  // UInt32, biped slot bits (JIP)
	constexpr std::uintptr_t kArmor_BipedFlags = 0x78;  // UInt32 (JIP)
	constexpr std::uint32_t kBipedFlag_PowerArmor = 0x20;

	constexpr std::uint32_t kBipedSlot_Head = 1u << 0;
	constexpr std::uint32_t kBipedSlot_Hair = 1u << 1;
	constexpr std::uint32_t kBipedSlot_Headband = 1u << 9;
	constexpr std::uint32_t kBipedSlot_Hat = 1u << 10;
	constexpr std::uint32_t kBipedSlot_Eyeglasses = 1u << 11;
	constexpr std::uint32_t kBipedSlot_Mask = 1u << 14;
	constexpr std::uint32_t kBipedSlot_MouthObject = 1u << 16;

	// TESObjectCELL (CELL)
	constexpr std::uintptr_t kCell_FullNameData = 0x1C;  // TESFullName@18 -> String.m_data (JIP)
	constexpr std::uintptr_t kCell_Flags = 0x24;  // UInt8, bit 0 = interior (JIP)
	constexpr std::uintptr_t kCell_ExtraDataHead = 0x2C;  // ExtraDataList@28 -> m_data (JIP)
	constexpr std::uintptr_t kCell_ObjectList = 0xAC;  // tList<TESObjectREFR>, first node inline (JIP)

	// tList node
	constexpr std::uintptr_t kListNode_Data = 0x00;
	constexpr std::uintptr_t kListNode_Next = 0x04;

	// Scene graph
	constexpr std::uintptr_t kSceneGraph_Camera = 0xAC;  // NiCamera* (JIP; NiNode is 0xAC bytes)
	constexpr std::uintptr_t kNiObject_Parent = 0x18;  // NiNode* (JIP NiAVObject)
	constexpr std::uintptr_t kNiObject_WorldRotate = 0x68;  // NiMatrix33, row major (JIP)
	constexpr std::uintptr_t kNiObject_WorldTranslate = 0x8C;  // NiVector3 (JIP)
	constexpr std::uintptr_t kVtbl_BSFadeNode = 0x10A8F90;  // (JIP)
	constexpr std::uintptr_t kFadeNode_Reference = 0xCC;  // TESObjectREFR*, can be null (JIP NiAVObject::GetParentRef)
	constexpr std::uintptr_t kNiObjectNET_Controller = 0x0C;  // NiTimeController*, first of a chain (JIP)
	constexpr std::uintptr_t kTimeController_Next = 0x30;  // NiTimeController* (JIP)
	constexpr std::uintptr_t kVtbl_NiControllerManager = 0x109619C;  // Animated objects (open/close sequences) (JIP)

	// Havok
	constexpr float kHavokScale = 0.1428571f;  // Game units -> Havok units (1/7) (JIP kUnitConv)

	// bool __thiscall bhkWorld::PickObject(PickData*). TES::PickObject reaches object through the cell (0x553EE0).
	// Both vtables point to 0xC696D0. (JIP havok.h slot /*C8*/; EXE: ret 4)
	constexpr std::uintptr_t kVtbl_bhkWorld = 0x10C40B4;  // (JIP)
	constexpr std::uintptr_t kVtbl_bhkWorldM = 0x10C69F4;  // (JIP)
	constexpr std::uintptr_t kWorldVtbl_PickObject = 0xC8;
	constexpr std::uintptr_t kCdBody_Parent = 0x0C;  // hkCdBody*, is null for the root collidable (JIP)
	constexpr std::uintptr_t kRootCdBody_Layer = 0x1C;  // Root hkCdBody is hkpWorldObject +0x10. Layer at hkpWorldObject +0x2C. (JIP)

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
	constexpr std::uintptr_t kSoundVtbl_Update = 0x44;  // bool(DWORD timeDelta) (JG slot 17; EXE: ret 4)
	constexpr std::uintptr_t kSoundVtbl_GetEmitterPosition = 0x54;  // void(NiPoint3&) (JG slot 21; EXE: ret 4, reads +0x218)

	constexpr std::uintptr_t kSound_ID = 0x004;
	constexpr std::uintptr_t kSound_TypeFlags = 0x008;  // SoundFlag bits (JG, JIP)
	constexpr std::uintptr_t kSound_StateFlags = 0x010;
	constexpr std::uintptr_t kSound_StaticAttenuation = 0x018;  // UInt16, unit not verified (JIP: "dB * -1000")
	constexpr std::uintptr_t kSound_ReverbAttenuation = 0x01A;  // UInt16 (JG: usReverbAttenuation)
	constexpr std::uintptr_t kSound_FilePath = 0x036;  // char[]
	constexpr std::uintptr_t kSound_SourceSound = 0x134;  // TESSound*, set only when JIP's patch is active (JIP)
	constexpr std::uintptr_t kSound_MaxAttenuationDistance = 0x13C;  // float, game units (JIP)
	constexpr std::uintptr_t kSound_MinAttenuationDistance = 0x140;  // float, game units (JIP)
	constexpr std::uintptr_t kSound_EnvironmentType = 0x154;  // UInt32 (EXE: SetEnvironmentType writes it)

	// Infrastructure
	// +1CC holds the same object as +19C.
	constexpr std::uintptr_t kWin32Sound_Device = 0x198;    // IDirectSound8*
	constexpr std::uintptr_t kWin32Sound_Buffer = 0x19C;    // IDirectSoundBuffer8* (2D and 3D). Gives IKsPropertySet with DSOAL.
	constexpr std::uintptr_t kWin32Sound_Buffer3D = 0x1D4;  // IDirectSound3DBuffer*, 3D sounds only
	constexpr std::uintptr_t kWin32Sound_EmitterPosition = 0x218;  // NiPoint3. GetEmitterPosition reads it for 3D and 2DRadius sounds. (EXE)
	constexpr std::uintptr_t kWin32Sound_ProbeBegin = 0x198;  // Layout probe scans this range for DirectSound COM pointers
	constexpr std::uintptr_t kWin32Sound_ProbeEnd = 0x230;    // Object size from JIP. ST gives 0x2E0.

}
