#pragma once

#include "occlusion/probe.h"

#include <string>

namespace sea::occlusion {

	std::string DescriptionForReference(void* reference);
	std::string DescriptionForEmitter(void* reference);
	std::string DescriptionForOccluder(const Occluder& occluder);
	std::string DescriptionForLayer(const OcclusionLayer& layer);
	std::string DescriptionForOcclusion(const ProbeResult& result);

}
