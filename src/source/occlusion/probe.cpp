#include "occlusion/probe.h"

#include "engine/addresses.h"
#include "engine/havok.h"
#include "engine/objects.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace sea::occlusion {

	namespace {

		constexpr int kMaxHitsPerDirection = kMaxCastsPerProbe / 2 - 1;
		constexpr float kStepPastHit = 1.0f;  // Game units

		// Hits this close to the sound are the surface the source is mounted on or the floor under it.
		// A wall on the far side of the source is usually further away than this.
		constexpr float kSourceIgnoreRadius = 12.0f;  // Game units

		// References with their origin this close to the sound are the emitter (`PlaySound3D`, placed emitters).
		constexpr float kEmitterOriginRadius = 8.0f;  // Game units

		// Objects this close to each other along the ray form one layer: seams, back-to-back walls, a door in its frame.
		// Back-to-back kit walls were seen 48 units apart along the ray.
		constexpr float kLayerMergeGap = 64.0f;  // Game units

		// References with a smaller horizontal size never occlude: clutter, poles, small props.
		constexpr float kMinOccluderSize = 35.0f;  // Game units, about half a meter

		// Share of `fWallLevel` for a layer made of doors only.
		constexpr float kDoorWeight = 0.5f;

		// The engine's AI sight ray. In Phase 0 all ray layers gave the same hits. Filtering is done on the hits instead.
		constexpr std::uint8_t kRayLayer = engine::kLayer_LineOfSight;

		constexpr std::size_t kMaxDescribedLayers = 4;
		constexpr std::size_t kMaxDescribedReferences = 3;

		// The part of the ray one reference covers, from its nearest to its furthest hit.
		struct Span {
			void* reference = nullptr;
			float nearDistance = 0.0f;  // Game units from listener
			float farDistance = 0.0f;  // Game units from listener
			float weight = 1.0f;
			std::uint8_t collisionLayer = 0;  // Of the first hit, for the log
		};

		// A physical layer between listener and sound, made of spans that overlap or nearly touch.
		//
		// The layer takes the highest weight of its spans. A ray that hits a door and a wall went through the wall.
		struct Layer {
			float nearDistance = 0.0f;  // Game units from listener
			float farDistance = 0.0f;  // Game units from listener
			float weight = 0.0f;
			std::vector<void*> references;
			std::vector<std::uint8_t> collisionLayers;  // Per reference
		};

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

		// Checks if the given reference is too small to occlude.
		// Uses the larger horizontal side of the base form's object bounds times the reference's scale.
		// Forms without bounds are never too small.
		bool IsTooSmallToOcclude(void* reference) {
			const Vector3 boundsSize = engine::GetBoundsSize(engine::GetBaseForm(reference));
			const float horizontalSize = std::max(boundsSize.x, boundsSize.y);

			if (horizontalSize <= 0.0f) {
				return false;
			}

			return horizontalSize * engine::GetScale(reference) < kMinOccluderSize;
		}

		bool IsActor(void* reference) {
			const std::uint8_t formType = engine::GetFormType(reference);

			return formType == engine::kFormType_Character || formType == engine::kFormType_Creature;
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
		// Optionally stops at the first hit on the emitter of a sound at the end point.
		void CastThrough(const Vector3& from, const Vector3& to, std::vector<engine::RaycastResult>& hits, int& casts,
			bool stopAtEmitter) {
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

				if (stopAtEmitter && IsEmitter(hit.reference, to)) {
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
			const std::uint8_t formType = engine::GetFormType(engine::GetBaseForm(reference));

			if (formType == engine::kFormType_TESObjectDOOR) {
				return kDoorWeight;
			}

			return 1.0f;
		}

		bool Contains(const std::vector<void*>& references, void* reference) {
			return std::find(references.begin(), references.end(), reference) != references.end();
		}

		// Widens the span of the given reference to include a hit at the given distance. Adds a span if there is none.
		void AddToSpans(std::vector<Span>& spans, void* reference, float distance, std::uint8_t collisionLayer) {
			for (Span& span : spans) {
				if (span.reference == reference) {
					span.nearDistance = std::min(span.nearDistance, distance);
					span.farDistance = std::max(span.farDistance, distance);

					return;
				}
			}

			spans.push_back({reference, distance, distance, GetOccluderWeight(reference), collisionLayer});
		}

		// Merges spans into layers. Spans that overlap or lie within `kLayerMergeGap` of each other share a layer.
		std::vector<Layer> MergeSpans(std::vector<Span>& spans) {
			std::sort(spans.begin(), spans.end(), [](const Span& left, const Span& right) {
				return left.nearDistance < right.nearDistance;
			});

			std::vector<Layer> layers;

			for (const Span& span : spans) {
				if (layers.empty() || span.nearDistance > layers.back().farDistance + kLayerMergeGap) {
					layers.push_back({span.nearDistance, span.farDistance, 0.0f, {}, {}});
				}

				Layer& layer = layers.back();
				layer.farDistance = std::max(layer.farDistance, span.farDistance);
				layer.weight = std::max(layer.weight, span.weight);
				layer.references.push_back(span.reference);
				layer.collisionLayers.push_back(span.collisionLayer);
			}

			return layers;
		}

		// Appends text to the log description of the given result. Cuts off text that does not fit.
		void AppendDescription(ProbeResult& result, const char* text) {
			const std::size_t usedLength = std::strlen(result.description);
			std::snprintf(result.description + usedLength, sizeof(result.description) - usedLength, "%s", text);
		}

		// Appends the editor ID of the base form of the given reference to the log description.
		// Falls back to form type and ID.
		void AppendReferenceName(ProbeResult& result, void* reference) {
			void* baseForm = engine::GetBaseForm(reference);
			const char* editorID = engine::GetEditorID(baseForm);

			// Editor IDs need JohnnyGuitar NVSE for most forms.
			if (editorID[0] == '\0') {
				char fallbackName[32];
				std::snprintf(fallbackName, sizeof(fallbackName), "%s %08X", engine::FormTypeName(engine::GetFormType(baseForm)),
					engine::GetFormID(baseForm));
				AppendDescription(result, fallbackName);

				return;
			}

			AppendDescription(result, editorID);
		}

		// Appends a layer to the log description as its references joined by "+" and its distance from the listener.
		void AppendLayerDescription(ProbeResult& result, const Layer& layer) {
			if (result.description[0] != '\0') {
				AppendDescription(result, ", ");
			}

			for (std::size_t index = 0; index < layer.references.size() && index < kMaxDescribedReferences; ++index) {
				if (index > 0) {
					AppendDescription(result, "+");
				}

				AppendReferenceName(result, layer.references[index]);

				char collisionLayerText[32];
				std::snprintf(collisionLayerText, sizeof(collisionLayerText), "[%s]",
					engine::CollisionLayerName(layer.collisionLayers[index]));
				AppendDescription(result, collisionLayerText);
			}

			char distanceText[32];
			std::snprintf(distanceText, sizeof(distanceText), " @%.0f x%.1f", layer.nearDistance, layer.weight);
			AppendDescription(result, distanceText);
		}

	}

	// Counts physical layers between listener and sound. Casts from listener to sound and back past each hit.
	// Hits of one reference form a span. Spans that overlap or nearly touch form one layer.
	//
	// Counts hits on static, animated static, transparent, props, terrain and ground layers only.
	// Skips actors, references smaller than `kMinOccluderSize`, the emitter's own reference and hits near the sound.
	//
	// Line of sight ends where the ray enters the emitter. Hits past that point are inside or behind the source.
	// Emitter origins can sit inside a wall or under the floor.
	//
	// Thread: Main
	ProbeResult ProbeOcclusion(const Vector3& listener, const Vector3& source) {
		ProbeResult result;
		std::vector<engine::RaycastResult> hits;

		CastThrough(listener, source, hits, result.casts, true);
		CastThrough(source, listener, hits, result.casts, false);

		std::vector<void*> emitterReferences;
		float emitterDistance = std::numeric_limits<float>::max();  // Game units from listener to nearest emitter hit

		for (const engine::RaycastResult& hit : hits) {
			if (!IsEmitter(hit.reference, source)) {
				continue;
			}

			emitterDistance = std::min(emitterDistance, Distance(listener, hit.point));

			if (!Contains(emitterReferences, hit.reference)) {
				emitterReferences.push_back(hit.reference);
			}
		}

		std::vector<Span> spans;
		bool hasTerrain = false;
		std::uint8_t terrainCollisionLayer = 0;  // Of the first terrain hit, for the log

		for (const engine::RaycastResult& hit : hits) {
			if (Distance(hit.point, source) <= kSourceIgnoreRadius || Distance(listener, hit.point) > emitterDistance) {
				continue;
			}

			// Terrain counts once. A span from ridge to ridge would swallow everything between.
			// Terrain skips the layer check until its layer is known from the log.
			if (!hit.reference) {
				if (!hasTerrain) {
					terrainCollisionLayer = hit.layer;
				}

				hasTerrain = true;
				continue;
			}

			if (!IsOccluderLayer(hit.layer)) {
				continue;
			}

			if (IsActor(hit.reference) || Contains(emitterReferences, hit.reference) || IsTooSmallToOcclude(hit.reference)) {
				continue;
			}

			AddToSpans(spans, hit.reference, Distance(listener, hit.point), hit.layer);
		}

		const std::vector<Layer> layers = MergeSpans(spans);

		for (std::size_t index = 0; index < layers.size() && index < kMaxDescribedLayers; ++index) {
			AppendLayerDescription(result, layers[index]);
		}

		for (const Layer& layer : layers) {
			result.weight += layer.weight;
		}

		result.layers = static_cast<int>(layers.size());

		if (hasTerrain) {
			if (result.description[0] != '\0') {
				AppendDescription(result, ", ");
			}

			char terrainText[48];
			std::snprintf(terrainText, sizeof(terrainText), "terrain[%s]", engine::CollisionLayerName(terrainCollisionLayer));
			AppendDescription(result, terrainText);
			++result.layers;
			result.weight += 1.0f;
		}

		if (!emitterReferences.empty()) {
			AppendDescription(result, "; emitter ");
			AppendReferenceName(result, emitterReferences.front());
		}

		return result;
	}

}
