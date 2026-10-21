#include "occlusion/probe.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/havok.h"
#include "engine/objects.h"
#include "occlusion/object_lists.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace sea::occlusion {

	namespace {

		constexpr int kMaxHitsPerDirection = kMaxCastsPerProbe / 2 - 1;

		constexpr float kStepPastHit = 1.0f;  // Game units

		// Hits this close to the sound are the surface the source is mounted on or the floor under it.
		// A wall on the far side of the source should be further away than this.
		constexpr float kSourceIgnoreRadius = 12.0f;  // Game units

		// References with their origin this close to the sound are the emitter (`PlaySound3D`, placed emitters).
		constexpr float kEmitterOriginRadius = 8.0f;  // Game units

		// Objects this close to each other along the ray form one layer: seams, back-to-back walls, a door in its frame.
		// Back-to-back kit walls were seen 48 units apart along the ray.
		constexpr float kLayerMergeGap = 64.0f;  // Game units

		// Share of `fWallLevel` for a layer made of doors only.
		constexpr float kDoorWeight = 0.5f;

		// The engine's AI sight ray. In Phase 0 all ray layers gave the same hits. Filtering is done on the hits instead.
		constexpr std::uint8_t kRayLayer = engine::kLayer_LineOfSight;

		// Checks if a hit on the given collision layer can occlude.
		// Layer 0 is also the result of a failed layer read and never occludes.
		bool IsOccluderLayer(std::uint8_t layer) {
			switch (layer) {
				case engine::kLayer_Static:
				case engine::kLayer_AnimStatic:
				case engine::kLayer_Transparent:
				case engine::kLayer_Props:
				case engine::kLayer_Terrain:
				case engine::kLayer_Ground:
					return true;
				default:
					return false;
			}
		}

		bool IsDoor(void* reference) {
			return engine::GetFormType(engine::GetBaseForm(reference)) == engine::kFormType_TESObjectDOOR;
		}

		// Checks if the given reference is smaller than `fMinOccluderSize`: furniture, clutter, poles.
		// Doors always occlude. A closed door alone fills its frame's opening.
		// Objects without usable bounds are not filtered here.
		bool IsTooSmallToOcclude(void* reference) {
			if (IsDoor(reference)) {
				return false;
			}

			const float horizontalSize = OccluderSize(reference);

			if (horizontalSize <= 0.0f) {
				return false;
			}

			return horizontalSize < config::Get().occlusion.minOccluderSize;
		}

		bool IsActor(void* reference) {
			const std::uint8_t formType = engine::GetFormType(reference);
			bool isActor = formType == engine::kFormType_Character || formType == engine::kFormType_Creature;

			return isActor;
		}

		// Checks if the given reference is the emitter of a sound at the given position.
		// The engine plays object sounds at the origin of the emitter reference.
		bool IsEmitter(void* reference, const Vector3& sound) {
			if (!reference) {
				return false;
			}

			return Distance(engine::GetPosition(reference), sound) <= kEmitterOriginRadius;
		}

		// Casts between two points and again from just past each hit. Adds every hit to given hits.
		// Optionally stops at the first hit on the emitter of the sound at the given sound position.
		void CastThrough(const Vector3& from, const Vector3& to, const Vector3& sound, std::vector<engine::RaycastResult>& hits,
			int& casts, bool stopAtEmitter) {
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

				if (stopAtEmitter && IsEmitter(hit.reference, sound)) {
					return;
				}

				start = hit.point + step;

				if (Distance(from, start) >= totalLength) {
					return;
				}
			}
		}

		// Returns the share of `fWallLevel` that the given reference adds as an occluder.
		float GetOccluderWeight(void* reference) {
			if (IsDoor(reference)) {
				return kDoorWeight;
			}

			return 1.0f;
		}

		// Widens the occluder of the given reference to include a hit at the given distance.
		// Adds an occluder if there is none.
		void AddToOccluders(std::vector<Occluder>& occluders, void* reference, float distance, std::uint8_t collisionLayer) {
			for (Occluder& occluder : occluders) {
				if (occluder.reference == reference) {
					occluder.nearDistance = std::min(occluder.nearDistance, distance);
					occluder.farDistance = std::max(occluder.farDistance, distance);

					return;
				}
			}

			occluders.push_back({reference, collisionLayer, distance, distance, GetOccluderWeight(reference)});
		}

		// Merges occluders into layers. Occluders that overlap or lie within `kLayerMergeGap` of each other share a layer.
		std::vector<OcclusionLayer> MergeOccluders(std::vector<Occluder>& occluders) {
			std::sort(occluders.begin(), occluders.end(), [](const Occluder& left, const Occluder& right) {
				return left.nearDistance < right.nearDistance;
			});

			std::vector<OcclusionLayer> layers;

			for (const Occluder& occluder : occluders) {
				if (layers.empty() || occluder.nearDistance > layers.back().farDistance + kLayerMergeGap) {
					layers.push_back({{}, occluder.nearDistance, occluder.farDistance, 0.0f});
				}

				OcclusionLayer& layer = layers.back();

				layer.farDistance = std::max(layer.farDistance, occluder.farDistance);
				layer.weight = std::max(layer.weight, occluder.weight);
				layer.occluders.push_back(occluder);
			}

			return layers;
		}

		// Inserts a terrain layer at its distance among the given layers, which are sorted by distance.
		void InsertTerrainLayer(std::vector<OcclusionLayer>& layers, const Occluder& terrain) {
			const auto position = std::find_if(layers.begin(), layers.end(), [&terrain](const OcclusionLayer& layer) {
				return layer.nearDistance > terrain.nearDistance;
			});

			layers.insert(position, {{terrain}, terrain.nearDistance, terrain.farDistance, terrain.weight});
		}

	}

	// Returns the larger horizontal side of the base form's object bounds times the scale of the given reference.
	// Forms can lack object bounds (return as 0 for all axes). Fall back to diameter of world bound sphere.
	float OccluderSize(void* reference) {
		const Vector3 boundsSize = engine::GetBoundsSize(engine::GetBaseForm(reference));
		const float horizontalSize = std::max(boundsSize.x, boundsSize.y);

		if (horizontalSize > 0.0f) {
			return horizontalSize * engine::GetScale(reference);
		}

		Vector3 center;
		float radius = 0.0f;

		if (!engine::GetWorldBound(reference, center, radius)) {
			return 0.0f;
		}

		return radius * 2.0f;
	}

	// Counts physical layers between listener and sound. Casts from listener to sound and back past each hit.
	// Casts end at the world bound center of the given emitter of the last probe if its origin is still at sound position.
	// Multiple occluders in close proximity may be merged into single layer.
	//
	// Counts hits on static, animated static, transparent, props, terrain and ground layers only.
	// Skips actors, references smaller than `fMinOccluderSize`, the emitter's own reference and hits near the sound.
	// Skips objects in `[OcclusionOccluderBlacklist]`. Sounds of an emitter in `[OcclusionEmitterBlacklist]` are never occluded.
	//
	// Line of sight ends where the ray enters the emitter. Hits past that point are inside or behind the source.
	// Emitter origins can sit inside a wall or under the floor.
	//
	// Thread: Main
	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source, void* knownEmitter) {
		ProbeResult result;

		if (!IsEmitter(knownEmitter, source)) {
			knownEmitter = nullptr;
		}

		if (IsExcludedEmitter(knownEmitter)) {
			result.emitter = knownEmitter;
			result.emitterExcluded = true;
			return result;
		}

		Vector3 target = source;
		Vector3 emitterCenter;
		float emitterRadius = 0.0f;

		if (knownEmitter && engine::GetWorldBound(knownEmitter, emitterCenter, emitterRadius)) {
			target = emitterCenter;
		}

		std::vector<engine::RaycastResult> hits;

		CastThrough(listener, target, source, hits, result.casts, true);
		CastThrough(target, listener, source, hits, result.casts, false);

		float emitterDistance = std::numeric_limits<float>::max();  // Game units from listener to nearest emitter hit

		for (const engine::RaycastResult& hit : hits) {
			if (!IsEmitter(hit.reference, source)) {
				continue;
			}

			const float distance = Distance(listener, hit.point);

			if (distance < emitterDistance) {
				emitterDistance = distance;
				result.emitter = hit.reference;
			}
		}

		// Rays to the center can miss the emitter's collision.
		if (!result.emitter) {
			result.emitter = knownEmitter;
		}

		if (IsExcludedEmitter(result.emitter)) {
			result.emitterExcluded = true;
			return result;
		}

		std::vector<Occluder> occluders;
		bool hasTerrain = false;
		Occluder terrain;
		terrain.nearDistance = std::numeric_limits<float>::max();

		for (const engine::RaycastResult& hit : hits) {
			const float distance = Distance(listener, hit.point);

			if (Distance(hit.point, target) <= kSourceIgnoreRadius || distance > emitterDistance) {
				continue;
			}

			// Terrain handling
			if (!hit.reference) {
				if (distance < terrain.nearDistance) {
					terrain.nearDistance = distance;
					terrain.farDistance = distance;
					terrain.collisionLayer = hit.layer;
				}

				hasTerrain = true;
				continue;
			}

			if (!IsOccluderLayer(hit.layer)) {
				continue;
			}

			if (IsActor(hit.reference) || IsEmitter(hit.reference, source) || IsTooSmallToOcclude(hit.reference)) {
				continue;
			}

			if (IsBlacklistedOccluder(hit.reference)) {
				continue;
			}

			AddToOccluders(occluders, hit.reference, distance, hit.layer);
		}

		result.layers = MergeOccluders(occluders);

		if (hasTerrain) {
			InsertTerrainLayer(result.layers, terrain);
		}

		for (const OcclusionLayer& layer : result.layers) {
			result.weight += layer.weight;
		}

		return result;
	}

}
