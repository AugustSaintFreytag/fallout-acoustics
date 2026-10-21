// Object lists of `SaintsAcousticsForms.ini`, listed by editor ID of the base form.
//
// `[OcclusionOccluderBlacklist]`: Objects that never occlude sound.
// `[OcclusionEmitterBlacklist]`: Emitters whose sounds are never occluded.
//
// Editor IDs resolve to base form IDs once the game data is loaded. Most need JohnnyGuitar NVSE.

#include "occlusion/object_lists.h"

#include "config/settings.h"
#include "engine/objects.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace sea::occlusion {

	namespace {

		// Editor IDs without a form listed in the log, per list.
		constexpr std::size_t kMaxLoggedMissing = 16;

		// Blacklist of forms that should not occlude sounds.
		std::unordered_set<std::uint32_t> g_occlusionOcclusionOccluderBlacklist;

		// Blacklist of forms whose emitted sounds should not be occluded.
		std::unordered_set<std::uint32_t> g_occlusionEmitterBlacklist;

		// Reads the loaded ids of the given section.
		std::unordered_set<std::uint32_t> ResolveObjectList(const char* section) {
			const std::vector<std::string> editorIDs = config::ReadObjectList(section);
			std::unordered_set<std::uint32_t> formIDs;
			std::size_t missing = 0;

			for (const std::string& editorID : editorIDs) {
				void* form = engine::GetFormByEditorID(editorID.c_str());

				if (form) {
					formIDs.insert(engine::GetFormID(form));
					continue;
				}

				if (++missing <= kMaxLoggedMissing) {
					SEA_LOG("[Objects] Could not find form for editor id '%s' in [%s], will be skipped.", editorID.c_str(), section);
				}
			}

			SEA_LOG("[Objects] Resolved %zu of %zu editor ids in [%s].", formIDs.size(), editorIDs.size(), section);

			return formIDs;
		}

		bool ContainsBaseForm(const std::unordered_set<std::uint32_t>& formIDs, void* reference) {
			if (formIDs.empty() || !reference) {
				return false;
			}

			return formIDs.count(engine::GetFormID(engine::GetBaseForm(reference))) != 0;
		}

	}

	// Reads and resolves both object lists.
	//
	// Thread: Main
	void LoadObjectLists() {
		g_occlusionOcclusionOccluderBlacklist = ResolveObjectList("OcclusionOccluderBlacklist");
		g_occlusionEmitterBlacklist = ResolveObjectList("OcclusionEmitterBlacklist");
	}

	// Checks if the base form of the given reference is in the occluder blacklist.
	//
	// Thread: Main
	bool IsBlacklistedOccluder(void* reference) {
		return ContainsBaseForm(g_occlusionOcclusionOccluderBlacklist, reference);
	}

	// Checks if the base form of the given reference is in the emitter blacklist.
	//
	// Thread: Main
	bool IsExcludedEmitter(void* reference) {
		return ContainsBaseForm(g_occlusionEmitterBlacklist, reference);
	}

}
