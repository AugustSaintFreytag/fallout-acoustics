#include "occlusion/description.h"

#include "engine/havok.h"
#include "engine/objects.h"

#include <cstddef>
#include <cstdio>

namespace sea::occlusion {

	namespace {

		constexpr std::size_t kMaxDescribedLayers = 4;
		constexpr std::size_t kMaxDescribedOccluders = 3;

	}

	std::string DescriptionForReference(void* reference) {
		void* baseForm = engine::GetBaseForm(reference);
		const char* editorID = engine::GetEditorID(baseForm);

		if (editorID[0] != '\0') {
			return editorID;
		}

		char fallbackName[32];
		
		std::snprintf(
			fallbackName, 
			sizeof(fallbackName), 
			"%s %08X", 
			engine::FormTypeName(engine::GetFormType(baseForm)),
			engine::GetFormID(baseForm)
		);

		return fallbackName;
	}

	std::string DescriptionForEmitter(void* reference) {
		if (!reference) {
			return "unknown";
		}

		return "\"" + DescriptionForReference(reference) + "\"";
	}

	std::string DescriptionForOccluder(const Occluder& occluder) {
		const std::string collisionLayer = engine::CollisionLayerName(occluder.collisionLayer);

		if (!occluder.reference) {
			return collisionLayer + ":Terrain";
		}

		char sizeText[32];
		std::snprintf(sizeText, sizeof(sizeText), "[%.0fu]", OccluderSize(occluder.reference));

		return collisionLayer + ":\"" + DescriptionForReference(occluder.reference) + "\"" + sizeText;
	}

	std::string DescriptionForLayer(const OcclusionLayer& layer) {
		std::string description;

		for (std::size_t index = 0; index < layer.occluders.size() && index < kMaxDescribedOccluders; ++index) {
			if (index > 0) {
				description += "+";
			}

			description += DescriptionForOccluder(layer.occluders[index]);
		}

		char distanceText[32];
		std::snprintf(distanceText, sizeof(distanceText), "@%.0fu", layer.nearDistance);

		return description + distanceText;
	}

	std::string DescriptionForOcclusion(const ProbeResult& result) {
		if (result.emitterExcluded) {
			return "excluded";
		}

		if (result.weight <= 0.0f) {
			return "none";
		}

		char weightText[32];
		std::snprintf(weightText, sizeof(weightText), "%.1f (", result.weight);

		std::string description = weightText;

		for (std::size_t index = 0; index < result.layers.size() && index < kMaxDescribedLayers; ++index) {
			if (index > 0) {
				description += ", ";
			}

			description += DescriptionForLayer(result.layers[index]);
		}

		return description + ")";
	}

}
