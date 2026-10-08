#pragma once

#include <cstdint>

namespace sea::debug {

	void OnEnginePick(std::uintptr_t caller);

	void PollPickCensus();

}
