#pragma once

namespace sea::debug {

	bool ToggleTestOcclusion();

	void ApplyTestOcclusionOnPlay(void* sound);

	void ApplyTestOcclusionOnUpdate(void* sound);

}
