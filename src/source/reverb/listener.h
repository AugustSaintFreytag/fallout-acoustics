#pragma once

#include "utils/vector3.h"

#include <cstdint>

namespace sea::reverb {

	void UpdateListenerEnvironment();

	std::uint32_t GetListenerEnvironment();

	bool IsListenerInExterior();

	Vector3 GetListenerPosition();

}
