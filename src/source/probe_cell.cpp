#include "probe_cell.h"

#include "game.h"
#include "log.h"
#include "memory.h"

namespace sea::probe {
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

			const std::uint32_t environment = Field<std::uint32_t>(space, game::kAspc_EnvironmentType);

			SEA_LOG("  %-14s %08X '%s' Env=%u (%s) Interior=%u", label, game::GetFormID(space),
				game::GetEditorID(space), environment, game::EnvironmentTypeName(environment),
				Field<std::uint8_t>(space, game::kAspc_IsInterior));
		}
	}

	void PollPlayerAcoustics() {
		void* player = game::GetPlayer();

		if (!player) {
			return;
		}

		void* cell = game::GetParentCell(player);
		void* space = mem::Global<void*>(game::kCurrentAcousticSpace);
		void* cellSpace = mem::Global<void*>(game::kCurrentCellAcousticSpace);

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

		const char* fullName = Field<const char*>(cell, game::kCell_FullNameData);

		if (!fullName) {
			fullName = "";
		}

		SEA_LOG("[Acoustics] Cell %08X '%s' \"%s\" Interior=%u", game::GetFormID(cell), game::GetEditorID(cell),
			fullName, Field<std::uint8_t>(cell, game::kCell_Flags) & 1);

		LogSpace("Cell ASPC:", game::GetCellAcousticSpace(cell));
		LogSpace("Engine Current:", space);
		LogSpace("Engine Cell:", cellSpace);
	}

	void ResetPlayerAcoustics() {
		g_lastCell = nullptr;
		g_lastSpace = nullptr;
		g_lastCellSpace = nullptr;
	}
}
