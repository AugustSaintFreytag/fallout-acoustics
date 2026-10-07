#pragma once

#include <cstdint>

namespace sea::engine {
	// ANAM value = DSFX_I3DL2_ENVIRONMENT_PRESET_* + 1 (JIP BGSAcousticSpace).
	const char* EnvironmentTypeName(std::uint32_t type);

	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback); // Case insensitive
}
