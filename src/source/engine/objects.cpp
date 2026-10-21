#include "engine/objects.h"

#include "engine/addresses.h"
#include "utils/memory.h"

#include <cmath>
#include <cstdint>

namespace sea::engine {

	using mem::Field;

	namespace {

		const mem::ModuleRange& ExeRange() {
			static const mem::ModuleRange range = mem::GetModuleRange(nullptr);

			return range;
		}

	}

	void* GetPlayer() {
		return mem::Global<void*>(kPlayerSingleton);
	}

	// Returns the reference selected in the console, or null.
	void* GetConsoleSelection() {
		void* interfaceManager = mem::Global<void*>(kInterfaceManagerSingleton);

		if (!interfaceManager) {
			return nullptr;
		}

		return mem::Field<void*>(interfaceManager, kInterfaceManager_DebugSelection);
	}

	void* GetParentCell(void* reference) {
		if (!reference) {
			return nullptr;
		}

		return Field<void*>(reference, kRefr_ParentCell);
	}

	// Returns the acoustic space of a cell (`ExtraCellAcousticSpace`) or null.
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

	// Returns the editor ID of a form or empty string if not available. 
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

	// Returns the form with a given editor ID, or null.
	// Most forms are found only with JohnnyGuitar NVSE, which keeps their editor IDs.
	//
	// Thread: Main
	void* GetFormByEditorID(const char* editorID) {
		using GetFormByEditorIDFn = void*(__cdecl*)(const char* editorID);

		return reinterpret_cast<GetFormByEditorIDFn>(kForm_GetFormByEditorID)(editorID);
	}

	// Returns the reference with the given form ID. Returns null if there is none or the form is no reference.
	void* LookupReference(std::uint32_t formID) {
		if (formID == 0) {
			return nullptr;
		}

		using LookupFormByIDFn = void*(__cdecl*)(std::uint32_t formID);
		void* form = reinterpret_cast<LookupFormByIDFn>(kLookupFormByID)(formID);
		const std::uint8_t formType = GetFormType(form);

		if (formType != kFormType_TESObjectREFR && formType != kFormType_Character && formType != kFormType_Creature) {
			return nullptr;
		}

		return form;
	}

	std::uint32_t GetFormID(void* form) {
		if (!form) {
			return 0;
		}

		return Field<std::uint32_t>(form, kForm_RefID);
	}

	// Returns the form type ID, 0 for null.
	std::uint8_t GetFormType(void* form) {
		if (!form) {
			return 0;
		}

		return Field<std::uint8_t>(form, kForm_TypeID);
	}

	// Returns the short record name of a form type, e.g. "STAT". 
	// Returns string "?" for types unknown to plugin.
	const char* FormTypeName(std::uint8_t formType) {
		switch (formType) {
		case kFormType_TESObjectACTI:
			return "ACTI";

		case kFormType_BGSTerminal:
			return "TERM";

		case kFormType_TESObjectCONT:
			return "CONT";

		case kFormType_TESObjectDOOR:
			return "DOOR";

		case kFormType_TESObjectMISC:
			return "MISC";

		case kFormType_TESObjectSTAT:
			return "STAT";

		case kFormType_BGSStaticCollection:
			return "SCOL";

		case kFormType_BGSMovableStatic:
			return "MSTT";

		case kFormType_TESObjectTREE:
			return "TREE";

		case kFormType_TESFurniture:
			return "FURN";

		case kFormType_TESNPC:
			return "NPC_";

		case kFormType_TESCreature:
			return "CREA";

		case kFormType_TESObjectREFR:
			return "REFR";

		case kFormType_Character:
			return "ACHR";

		case kFormType_Creature:
			return "ACRE";

		default:
			return "?";
		}
	}

	void* GetBaseForm(void* reference) {
		if (!reference) {
			return nullptr;
		}

		return Field<void*>(reference, kRefr_BaseForm);
	}

	Vector3 GetPosition(void* reference) {
		if (!reference) {
			return {};
		}

		return Field<Vector3>(reference, kRefr_Position);
	}

	// Returns the rotation of a reference in radians.
	Vector3 GetRotation(void* reference) {
		if (!reference) {
			return {};
		}

		return Field<Vector3>(reference, kRefr_Rotation);
	}

	float GetScale(void* reference) {
		if (!reference) {
			return 1.0f;
		}

		return Field<float>(reference, kRefr_Scale);
	}

	// Returns the size of the object bounds (OBND) of a given base form along its local axes.
	// Size is in game units at a scale of 1. All zero if the form has no bounds set.
	Vector3 GetBoundsSize(void* baseForm) {
		if (!baseForm) {
			return {};
		}

		float axisSizes[3] = {};

		for (int axis = 0; axis < 3; ++axis) {
			const std::int16_t minimum = Field<std::int16_t>(baseForm, kBoundObject_Bounds + axis * 2);
			const std::int16_t maximum = Field<std::int16_t>(baseForm, kBoundObject_Bounds + (axis + 3) * 2);

			axisSizes[axis] = std::fabs(static_cast<float>(maximum) - static_cast<float>(minimum));
		}

		return {axisSizes[0], axisSizes[1], axisSizes[2]};
	}

	// Writes the world bound sphere of a reference's loaded 3D to the given center and radius.
	bool GetWorldBound(void* reference, Vector3& center, float& radius) {
		if (!reference) {
			return false;
		}

		void* renderState = Field<void*>(reference, kRefr_RenderState);

		if (!renderState) {
			return false;
		}

		void* rootNode = Field<void*>(renderState, kRenderState_RootNode);

		if (!rootNode) {
			return false;
		}

		void* bound = Field<void*>(rootNode, kNiObject_WorldBound);

		if (!bound) {
			return false;
		}

		center = Field<Vector3>(bound, kNiBound_Center);
		radius = Field<float>(bound, kNiBound_Radius);

		return radius > 0.0f;
	}

	// Returns the model path relative to "meshes\". 
	// Returns empty string if the form type has no model or the path is empty.
	const char* GetModelPath(void* baseForm) {
		std::uintptr_t modelOffset = 0;

		switch (GetFormType(baseForm)) {
		case kFormType_TESObjectSTAT:
		case kFormType_BGSStaticCollection:
		case kFormType_BGSMovableStatic:
		case kFormType_TESObjectTREE:
			modelOffset = kModelOffset_Static;
			break;

		case kFormType_TESObjectACTI:
		case kFormType_BGSTerminal:
		case kFormType_TESObjectDOOR:
		case kFormType_TESObjectMISC:
		case kFormType_TESFurniture:
			modelOffset = kModelOffset_Activator;
			break;

		case kFormType_TESObjectCONT:
			modelOffset = kModelOffset_Container;
			break;

		default:
			return "";
		}

		const char* path = Field<const char*>(baseForm, modelOffset + kModel_PathData);

		if (!path) {
			return "";
		}

		return path;
	}

	// Returns the references in the object list of a cell.
	//
	// Thread: Main
	std::vector<void*> GetCellReferences(void* cell) {
		std::vector<void*> references;

		if (!cell) {
			return references;
		}

		void* node = &Field<std::uint8_t>(cell, kCell_ObjectList);

		while (node) {
			void* reference = Field<void*>(node, kListNode_Data);

			if (reference) {
				references.push_back(reference);
			}

			node = Field<void*>(node, kListNode_Next);
		}

		return references;
	}

	// Returns the sound object for a form a game sound plays. 
	// Returns null if the value is not a valid `TESSound`.
	// 
	// Safe to use with unset or stale values.
	// Depends on JIP patch to be active.
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
