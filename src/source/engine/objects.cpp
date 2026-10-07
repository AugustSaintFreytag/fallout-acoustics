#include "engine/objects.h"

#include "engine/addresses.h"
#include "utils/memory.h"

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

	std::uint8_t GetFormType(void* form) {
		if (!form) {
			return 0;
		}

		return Field<std::uint8_t>(form, kForm_TypeID);
	}

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

	Vector3 GetRotation(void* reference) {
		if (!reference) {
			return {};
		}

		return Field<Vector3>(reference, kRefr_Rotation);
	}

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
