#pragma once

#include <cstdint>

namespace sea::debug {

	// A kind of sound request by `BSSoundHandle` function.
	enum class SoundRequest {
		Play,
		PlayAfter,
		FadeInPlay,
		SetPosition,
		SetObjectToFollow,
	};

	void OnSoundRequested(std::uint32_t soundId, SoundRequest request, std::uintptr_t caller);

	void OnSoundStarted(void* sound, const char* route);

}
