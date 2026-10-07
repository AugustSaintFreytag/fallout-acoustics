#pragma once

#include <cstdint>

namespace sea::probe {
	struct SoundProbeConfig {
		bool logPlay = true;
		bool logEnvironmentChange = true;

		// Scan first n num of played sounds for DirectSound COM pointers.
		std::uint32_t layoutProbeCount = 32;
	};

	// Patches the BSWin32GameSound vtable, should be called one time, from `NVSEPlugin_Load`.
	bool InstallSoundHooks(const SoundProbeConfig& config);
}
