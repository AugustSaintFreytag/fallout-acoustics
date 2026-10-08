#pragma once

#include <cstdint>

namespace sea::threads {

	void RememberMainThread();

	bool IsMainThread();

	void RememberAudioThread();

	bool IsAudioThread();

	std::uint32_t MainThreadId();
	std::uint32_t AudioThreadId();

}
