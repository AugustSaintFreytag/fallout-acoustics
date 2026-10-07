#pragma once

#include <cstdint>

namespace sea::debug {
	// Call one time, after `config::Load`.
	void InitializeSoundProbe();

	// Audio thread, after the original Play.
	void OnSoundPlayed(void* sound, bool loop, const char* route);

	// Audio thread, after the original SetEnvironmentType.
	void OnEnvironmentTypeChanged(void* sound, std::uint32_t previous, std::uint32_t type);
}
