#include "debug/grid_probe.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/objects.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/vector3.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sea::debug {

	using mem::Field;

	namespace {

		constexpr float kGridSizes[] = {32.0f, 64.0f, 128.0f, 256.0f, 512.0f};
		constexpr float kPositionTolerance = 1.0f;  // Game units
		constexpr float kRotationTolerance = 0.01f;  // Radians
		constexpr float kQuarterTurn = 1.5707963f;
		constexpr std::size_t kTopKitCount = 8;

		const char* const kKitPrefixes[] = {"architecture\\", "dungeons\\"};
		const char* const kMeshesPrefix = "meshes\\";

		// Thread: Main
		std::unordered_set<void*> g_analyzedCells;
		void* g_lastCell = nullptr;

		bool StartsWithNoCase(const char* text, const char* prefix) {
			return _strnicmp(text, prefix, std::strlen(prefix)) == 0;
		}

		const char* SkipMeshesPrefix(const char* path) {
			if (StartsWithNoCase(path, kMeshesPrefix)) {
				return path + std::strlen(kMeshesPrefix);
			}

			return path;
		}

		// Checks if a model is under `architecture\` or `dungeons\`.
		bool IsKitPiece(const char* modelPath) {
			const char* path = SkipMeshesPrefix(modelPath);

			for (const char* prefix : kKitPrefixes) {
				if (StartsWithNoCase(path, prefix)) {
					return true;
				}
			}

			return false;
		}

		// Returns first two folders of a model path like "dungeons\office".
		std::string KitName(const char* modelPath) {
			std::string path = SkipMeshesPrefix(modelPath);
			std::transform(path.begin(), path.end(), path.begin(), [](unsigned char character) {
				return static_cast<char>(std::tolower(character));
			});

			const std::size_t firstSeparator = path.find('\\');

			if (firstSeparator == std::string::npos) {
				return path;
			}

			const std::size_t secondSeparator = path.find('\\', firstSeparator + 1);

			if (secondSeparator == std::string::npos) {
				return path.substr(0, firstSeparator);
			}

			return path.substr(0, secondSeparator);
		}

		// Checks if a value lies on the grid within `kPositionTolerance`.
		bool IsAligned(float value, float gridSize) {
			const float nearest = std::round(value / gridSize) * gridSize;

			return std::fabs(value - nearest) <= kPositionTolerance;
		}

		bool IsQuarterTurn(float angle) {
			const float nearest = std::round(angle / kQuarterTurn) * kQuarterTurn;

			return std::fabs(angle - nearest) <= kRotationTolerance;
		}

		float Percent(std::size_t part, std::size_t total) {
			if (total == 0) {
				return 0.0f;
			}

			return 100.0f * static_cast<float>(part) / static_cast<float>(total);
		}

		// Logs kits with the most pieces.
		void LogKits(const std::map<std::string, std::size_t>& kitCounts) {
			std::vector<std::pair<std::string, std::size_t>> kits(kitCounts.begin(), kitCounts.end());

			std::sort(kits.begin(), kits.end(), [](const auto& left, const auto& right) {
				return left.second > right.second;
			});

			if (kits.size() > kTopKitCount) {
				kits.resize(kTopKitCount);
			}

			for (const auto& [kit, count] : kits) {
				SEA_LOG("[Grid]   Kit %-32s %zu pieces", kit.c_str(), count);
			}
		}

		// Logs alignment and rotation of static kit pieces in a cell. Also logs which kits they belong to.
		// Returns false if the cell has no references yet (not loaded).
		bool AnalyzeCell(void* cell) {
			const std::vector<void*> references = engine::GetCellReferences(cell);

			if (references.empty()) {
				return false;
			}

			std::vector<void*> kitPieces;
			std::map<std::string, std::size_t> kitCounts;
			std::size_t staticCount = 0;

			for (void* reference : references) {
				void* baseForm = engine::GetBaseForm(reference);

				if (engine::GetFormType(baseForm) != engine::kFormType_TESObjectSTAT) {
					continue;
				}

				++staticCount;

				const char* modelPath = engine::GetModelPath(baseForm);

				if (!IsKitPiece(modelPath)) {
					continue;
				}

				kitPieces.push_back(reference);
				++kitCounts[KitName(modelPath)];
			}

			const char* fullName = Field<const char*>(cell, engine::kCell_FullNameData);

			if (!fullName) {
				fullName = "";
			}

			SEA_LOG("[Grid] Cell %08X '%s' \"%s\": %zu references, %zu STAT, %zu kit pieces", engine::GetFormID(cell),
				engine::GetEditorID(cell), fullName, references.size(), staticCount, kitPieces.size());

			if (kitPieces.empty()) {
				return true;
			}

			for (const float gridSize : kGridSizes) {
				std::size_t alignedXY = 0;
				std::size_t alignedXYZ = 0;

				for (void* piece : kitPieces) {
					const Vector3 position = engine::GetPosition(piece);

					if (!IsAligned(position.x, gridSize) || !IsAligned(position.y, gridSize)) {
						continue;
					}

					++alignedXY;

					if (IsAligned(position.z, gridSize)) {
						++alignedXYZ;
					}
				}

				SEA_LOG("[Grid]   Grid %3.0f: %5.1f%% aligned in X/Y, %5.1f%% in X/Y/Z", gridSize,
					Percent(alignedXY, kitPieces.size()), Percent(alignedXYZ, kitPieces.size()));
			}

			std::size_t quarterTurns = 0;

			for (void* piece : kitPieces) {
				const Vector3 rotation = engine::GetRotation(piece);

				if (IsQuarterTurn(rotation.x) && IsQuarterTurn(rotation.y) && IsQuarterTurn(rotation.z)) {
					++quarterTurns;
				}
			}

			SEA_LOG("[Grid]   Rotation: %5.1f%% in steps of 90 degrees", Percent(quarterTurns, kitPieces.size()));

			LogKits(kitCounts);

			return true;
		}

	}

	// Logs how static architecture (kit pieces) of an interior cell aligns to grids of 32 to 512 units.
	// Also logs which kits the cell uses. Runs once per cell on first entry.
	//
	// Thread: Main (each frame)
	void PollGridProbe() {
		if (!config::Get().debug.probeGrid) {
			return;
		}

		void* cell = engine::GetParentCell(engine::GetPlayer());

		if (!cell || cell == g_lastCell) {
			return;
		}

		const bool isInterior = (Field<std::uint8_t>(cell, engine::kCell_Flags) & 1) != 0;

		if (!isInterior || g_analyzedCells.count(cell) > 0) {
			g_lastCell = cell;

			return;
		}

		// Try again on the next frame if the cell is not loaded yet.
		if (!AnalyzeCell(cell)) {
			return;
		}

		g_lastCell = cell;
		g_analyzedCells.insert(cell);
	}

}
