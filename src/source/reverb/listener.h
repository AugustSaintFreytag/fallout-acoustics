#pragma once

#include <cstdint>

namespace sea::reverb {
	// Main thread, run once on every frame.
	void UpdateListenerEnvironment();

	// Any thread. ANAM from the last update, 0 = unknown.
	std::uint32_t GetListenerEnvironment();
}
