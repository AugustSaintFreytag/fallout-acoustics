#pragma once

namespace sea::occlusion {

	void LoadObjectLists();

	bool IsBlacklistedOccluder(void* reference);
	
	bool IsExcludedEmitter(void* reference);

}
