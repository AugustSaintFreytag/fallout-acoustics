#pragma once

#include <cstdint>

namespace sea::debug {

	void InitializeSoundProbe();

	void OnSoundPlayed(void* sound, bool loop, const char* route);

	void OnEnvironmentTypeChanged(void* sound, std::uint32_t previous, std::uint32_t type);

}
