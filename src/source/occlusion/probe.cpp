#include "occlusion/probe.h"

#include "engine/addresses.h"
#include "engine/havok.h"
#include "engine/objects.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace sea::occlusion {

	namespace {

		constexpr int kMaxHitsPerDirection = kMaxCastsPerProbe / 2 - 1;
		constexpr float kStepPastHit = 1.0f;  // Game units

		// Hits this close to the sound belong to the emitter (a radio, a generator) or the floor under it.
		constexpr float kSourceIgnoreRadius = 32.0f;

		// The engine's AI sight ray. In Phase 0 all ray layers gave the same hits. Filtering is done on the hits instead.
		constexpr std::uint8_t kRayLayer = engine::kLayer_LineOfSight;

		constexpr std::size_t kMaxDescribedOccluders = 3;

		// Checks if a layer never occludes: actors, trees, small physics objects and small transparent objects.
		bool IsIgnoredLayer(std::uint8_t layer) {
			switch (layer) {
			case engine::kLayer_Clutter:
			case engine::kLayer_Weapon:
			case engine::kLayer_Projectile:
			case engine::kLayer_Biped:
			case engine::kLayer_Trees:
			case engine::kLayer_DebrisSmall:
			case engine::kLayer_DebrisLarge:
			case engine::kLayer_ShellCasing:
			case engine::kLayer_TransparentSmall:
			case engine::kLayer_TransparentSmallAnim:
			case engine::kLayer_DeadBip:
			case engine::kLayer_CharController:
				return true;

			default:
				return false;
			}
		}

		bool IsActor(void* reference) {
			const std::uint8_t formType = engine::GetFormType(reference);

			return formType == engine::kFormType_Character || formType == engine::kFormType_Creature;
		}

		// Casts between two points and again from just past each hit. Adds every hit to given hits.
		void CastThrough(const Vector3& from, const Vector3& to, std::vector<engine::RaycastResult>& hits, int& casts) {
			const Vector3 step = Normalize(to - from) * kStepPastHit;
			const float totalLength = Distance(from, to);
			Vector3 start = from;

			for (int hitIndex = 0; hitIndex <= kMaxHitsPerDirection; ++hitIndex) {
				const engine::RaycastResult hit = engine::CastRay(start, to, kRayLayer);
				++casts;

				if (!hit.hit) {
					return;
				}

				hits.push_back(hit);
				start = hit.point + step;

				if (Distance(from, start) >= totalLength) {
					return;
				}
			}
		}

		bool Contains(const std::vector<void*>& references, void* reference) {
			return std::find(references.begin(), references.end(), reference) != references.end();
		}

		// Appends the editor ID of a reference's base form to the log text. Falls back to form type and ID. Null is terrain.
		void AppendDescription(ProbeResult& result, void* reference) {
			const std::size_t usedLength = std::strlen(result.description);
			const char* separator = "";

			if (usedLength > 0) {
				separator = ", ";
			}

			char* text = result.description + usedLength;
			const std::size_t size = sizeof(result.description) - usedLength;

			if (!reference) {
				std::snprintf(text, size, "%sterrain", separator);

				return;
			}

			void* baseForm = engine::GetBaseForm(reference);
			const char* editorID = engine::GetEditorID(baseForm);

			// Editor IDs need JohnnyGuitar NVSE for most forms.
			if (editorID[0] == '\0') {
				std::snprintf(text, size, "%s%s %08X", separator, engine::FormTypeName(engine::GetFormType(baseForm)),
					engine::GetFormID(baseForm));

				return;
			}

			std::snprintf(text, size, "%s%s", separator, editorID);
		}

	}

	// Counts objects between listener and sound. Casts from listener to sound and back past each hit.
	//
	// Does not count: actors, small physics objects (clutter, debris, casings), the emitter's own object
	// (every reference that has a hit near the sound), and hits near the sound.
	//
	// Thread: Main
	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source) {
		ProbeResult result;
		std::vector<engine::RaycastResult> hits;

		CastThrough(listener, source, hits, result.casts);
		CastThrough(source, listener, hits, result.casts);

		// The emitter's own object: every reference with a hit near the sound.
		std::vector<void*> emitterReferences;

		for (const engine::RaycastResult& hit : hits) {
			if (hit.reference && Distance(hit.point, source) <= kSourceIgnoreRadius && !Contains(emitterReferences, hit.reference)) {
				emitterReferences.push_back(hit.reference);
			}
		}

		std::vector<void*> occluders;
		bool hasTerrain = false;

		for (const engine::RaycastResult& hit : hits) {
			if (IsIgnoredLayer(hit.layer) || Distance(hit.point, source) <= kSourceIgnoreRadius) {
				continue;
			}

			if (!hit.reference) {
				hasTerrain = true;
				continue;
			}

			if (IsActor(hit.reference) || Contains(emitterReferences, hit.reference) || Contains(occluders, hit.reference)) {
				continue;
			}

			occluders.push_back(hit.reference);

			if (occluders.size() <= kMaxDescribedOccluders) {
				AppendDescription(result, hit.reference);
			}
		}

		if (hasTerrain) {
			AppendDescription(result, nullptr);
		}

		result.occluders = static_cast<int>(occluders.size());

		if (hasTerrain) {
			++result.occluders;
		}

		return result;
	}

}
