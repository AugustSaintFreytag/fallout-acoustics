#pragma once

#include "utils/vector3.h"

namespace sea::engine {

	bool GetCameraTransform(Vector3& position, Vector3& forward);
	void* GetParentReference(void* niObject);

}
