#include "debug/cell_probe.h"

#include "engine/addresses.h"
#include "engine/environment.h"
#include "engine/objects.h"
#include "utils/log.h"
#include "utils/memory.h"

namespace sea::debug {
	using mem::Field;

	namespace {
		void* g_lastCell = nullptr;
		void* g_lastSpace = nullptr;
		void* g_lastCellSpace = nullptr;

		void LogSpace(const char* label, void* space) {
			if (!space) {
				SEA_LOG("  %-14s (none)", label);

				return;
			}

			const std::uint32_t environment = Field<std::uint32_t>(space, engine::kAspc_EnvironmentType);

			SEA_LOG("  %-14s %08X '%s' Env=%u (%s) Interior=%u", label, engine::GetFormID(space),
				engine::GetEditorID(space), environment, engine::EnvironmentTypeName(environment),
				Field<std::uint8_t>(space, engine::kAspc_IsInterior));
		}
	}

	void PollPlayerAcoustics() {
		void* player = engine::GetPlayer();

		if (!player) {
			return;
		}

		void* cell = engine::GetParentCell(player);
		void* space = mem::Global<void*>(engine::kCurrentAcousticSpace);
		void* cellSpace = mem::Global<void*>(engine::kCurrentCellAcousticSpace);

		if (cell == g_lastCell && space == g_lastSpace && cellSpace == g_lastCellSpace) {
			return;
		}

		g_lastCell = cell;
		g_lastSpace = space;
		g_lastCellSpace = cellSpace;

		if (!cell) {
			SEA_LOG("[Acoustics] Player has no cell.");

			return;
		}

		const char* fullName = Field<const char*>(cell, engine::kCell_FullNameData);

		if (!fullName) {
			fullName = "";
		}

		SEA_LOG("[Acoustics] Cell %08X '%s' \"%s\" Interior=%u", engine::GetFormID(cell), engine::GetEditorID(cell),
			fullName, Field<std::uint8_t>(cell, engine::kCell_Flags) & 1);

		LogSpace("Cell ASPC:", engine::GetCellAcousticSpace(cell));
		LogSpace("Engine Current:", space);
		LogSpace("Engine Cell:", cellSpace);
	}

	void ResetPlayerAcoustics() {
		g_lastCell = nullptr;
		g_lastSpace = nullptr;
		g_lastCellSpace = nullptr;
	}
}
