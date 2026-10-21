#pragma once

#include "utils/vector3.h"

#include <cstdint>
#include <vector>

namespace sea::engine {

	void* GetPlayer();
	void* GetConsoleSelection();
	void* GetParentCell(void* reference);
	void* GetCellAcousticSpace(void* cell);

	const char* GetEditorID(void* form);
	void* GetFormByEditorID(const char* editorID);

	std::uint32_t GetFormID(void* form);
	std::uint8_t GetFormType(void* form);

	const char* FormTypeName(std::uint8_t formType);

	void* GetBaseForm(void* reference);
	Vector3 GetPosition(void* reference);
	Vector3 GetRotation(void* reference);
	float GetScale(void* reference);
	Vector3 GetBoundsSize(void* baseForm);

	const char* GetModelPath(void* baseForm);

	std::vector<void*> GetCellReferences(void* cell);

	void* GetSourceSoundChecked(void* gameSound);

}
