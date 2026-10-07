#include "game.h"
#include "memory.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <iterator>

namespace sea::game {
	using mem::Field;

	namespace {
		constexpr const char* kEnvironmentNames[] = {
			"None", "Default", "Generic", "PaddedCell", "Room", "Bathroom", "Livingroom",
			"StoneRoom", "Auditorium", "ConcertHall", "Cave", "Arena", "Hangar",
			"CarpetedHallway", "Hallway", "StoneCorridor", "Alley", "Forest", "City",
			"Mountains", "Quarry", "Plain", "ParkingLot", "SewerPipe", "Underwater",
			"SmallRoom", "MediumRoom", "LargeRoom", "MediumHall", "LargeHall", "Plate",
		};

		struct FlagName {
			std::uint32_t bit;
			const char* name;
		};

		constexpr FlagName kSoundFlagNames[] = {
			{kSound_2D, "2D"},
			{kSound_3D, "3D"},
			{kSound_Voice, "Voice"},
			{kSound_Footsteps, "Foot"},
			{kSound_Loop, "Loop"},
			{kSound_SystemSound, "System"},
			{kSound_RandomFrequency, "RandFreq"},
			{kSound_Battle, "Battle"},
			{kSound_OneShot, "OneShot"},
			{kSound_Music, "Music"},
			{kSound_Region, "Region"},
			{kSound_MuteSubmerged, "MuteSubmerged?"},
			{kSound_Impact, "Impact"},
			{kSound_DontCache, "NoCache"},
			{kSound_2DGunfire, "2DGunfire"},
			{kSound_FirstPerson, "1stPerson"},
			{kSound_Modulated, "Modulated"},
			{kSound_Radio, "Radio"},
			{kSound_IgnoreTimescale, "NoTimescale"},
			{kSound_EnvelopeFast, "EnvFast"},
			{kSound_EnvelopeSlow, "EnvSlow"},
			{kSound_2DRadius, "2DRadius"},
			{kSound_AnimationDriven, "AnimDriven"},
		};

		const mem::ModuleRange& ExeRange() {
			static const mem::ModuleRange range = mem::GetModuleRange(nullptr);

			return range;
		}

		const char* FlagSeparator(std::size_t usedLength) {
			if (usedLength == 0) {
				return "";
			}

			return "|";
		}
	}

	const char* EnvironmentTypeName(std::uint32_t type) {
		if (type >= std::size(kEnvironmentNames)) {
			return "?";
		}

		return kEnvironmentNames[type];
	}

	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback) {
		for (std::uint32_t type = 1; type < std::size(kEnvironmentNames); ++type) {
			if (_stricmp(name, kEnvironmentNames[type]) == 0) {
				return type;
			}
		}

		return fallback;
	}

	void ShowNotification(const char* message) {
		using QueueUIMessageFn = bool(__cdecl*)(const char* message, std::uint32_t emotion, const char* ddsPath,
			const char* soundName, float seconds, bool maybeNextToDisplay);

		const auto queueUIMessage = reinterpret_cast<QueueUIMessageFn>(0x7052F0);
		queueUIMessage(message, 0, nullptr, nullptr, 2.0f, false);
	}

	void DescribeSoundFlags(std::uint32_t flags, char* buffer, std::size_t size) {
		buffer[0] = '\0';

		std::size_t usedLength = 0;
		std::uint32_t knownFlags = 0;

		for (const auto& [bit, name] : kSoundFlagNames) {
			knownFlags |= bit;

			if (!(flags & bit)) {
				continue;
			}

			const int written = std::snprintf(buffer + usedLength, size - usedLength, "%s%s", FlagSeparator(usedLength), name);

			if (written < 0 || usedLength + written >= size) {
				return;
			}

			usedLength += written;
		}

		const std::uint32_t unknownFlags = flags & ~knownFlags;

		if (unknownFlags != 0) {
			std::snprintf(buffer + usedLength, size - usedLength, "%s0x%X", FlagSeparator(usedLength), unknownFlags);
		}
	}

	void* GetPlayer() {
		return mem::Global<void*>(kPlayerSingleton);
	}

	void* GetParentCell(void* reference) {
		if (!reference) {
			return nullptr;
		}

		return Field<void*>(reference, kRefr_ParentCell);
	}

	void* GetCellAcousticSpace(void* cell) {
		if (!cell) {
			return nullptr;
		}

		void* extraData = Field<void*>(cell, kCell_ExtraDataHead);

		while (extraData) {
			if (Field<std::uint8_t>(extraData, kExtra_Type) == kExtraType_CellAcousticSpace) {
				return Field<void*>(extraData, kExtraCellAcousticSpace_Space);
			}

			extraData = Field<void*>(extraData, kExtra_Next);
		}

		return nullptr;
	}

	const char* GetEditorID(void* form) {
		if (!form) {
			return "";
		}

		using GetEditorIDFn = const char*(__thiscall*)(void* form);

		void* vtable = Field<void*>(form, 0);

		const auto getEditorID = Field<GetEditorIDFn>(vtable, kFormVtbl_GetEditorID);
		const char* editorID = getEditorID(form);

		if (!editorID) {
			return "";
		}

		return editorID;
	}

	std::uint32_t GetFormID(void* form) {
		if (!form) {
			return 0;
		}

		return Field<std::uint32_t>(form, kForm_RefID);
	}

	void* GetSourceSoundChecked(void* gameSound) {
		std::uint32_t formAddress = 0;

		if (!mem::SafeRead32(&Field<std::uint8_t>(gameSound, kSound_SourceSound), formAddress) || formAddress == 0) {
			return nullptr;
		}

		void* form = reinterpret_cast<void*>(formAddress);
		std::uint32_t vtable = 0;

		if (!mem::SafeRead32(form, vtable) || !ExeRange().Contains(vtable)) {
			return nullptr;
		}

		std::uint32_t typeWord = 0;

		if (!mem::SafeRead32(&Field<std::uint8_t>(form, kForm_TypeID), typeWord)) {
			return nullptr;
		}

		const auto formType = static_cast<std::uint8_t>(typeWord & 0xFF);

		if (formType != kFormType_TESSound) {
			return nullptr;
		}

		return form;
	}
}
