#pragma once

#include <cstdint>

namespace sea::engine {

	const char* EnvironmentTypeName(std::uint32_t type);

	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback);

}
