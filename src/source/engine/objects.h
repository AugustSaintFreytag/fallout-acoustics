#pragma once

#include "utils/vector3.h"

#include <cstdint>
#include <vector>

namespace sea::engine {
	void* GetPlayer();
	void* GetParentCell(void* reference);
	void* GetCellAcousticSpace(void* cell);  // ExtraCellAcousticSpace, or null

	const char* GetEditorID(void* form);     // "" if not available
	std::uint32_t GetFormID(void* form);
	std::uint8_t GetFormType(void* form);    // 0 for null

	// Short record name of a form type, for example "STAT". "?" for types that the plugin does not use.
	const char* FormTypeName(std::uint8_t formType);

	void* GetBaseForm(void* reference);
	Vector3 GetPosition(void* reference);
	Vector3 GetRotation(void* reference);  // Radians

	// Model path relative to "meshes\". "" if the form type has no model or the path is empty.
	const char* GetModelPath(void* baseForm);

	// References in the object list of a cell. Main thread.
	std::vector<void*> GetCellReferences(void* cell);

	// Returns the TESSound* sound source at +0x134.
	// Returns null if value is not a valid `TESSound` object.
	void* GetSourceSoundChecked(void* gameSound);
}
