#pragma once

#include <cstdint>
#include <string>

namespace sea::config {
	constexpr float kSendOff = -100.0f;  // dB. A send at or below this level is considered off/muted.

	struct ReverbSettings {
		bool enabled = true;
		float wetLevelDb = 0.0f;  // FX slot 0 volume

		// dB, added to the room level of each preset.
		// Presets are at -10 dB. Result is limited to 0 dB.
		float roomBoostDb = 0.0f;

		std::uint32_t interiorFallback = 26;  // ANAM for an interior without an acoustic space (MediumRoom)
		std::uint32_t exteriorFallback = 18;  // ANAM for an exterior without an acoustic space (City)
	};

	// Send level of each sound category, in dB.
	struct SendSettings {
		float voice3D = 0.0f;
		float voice2D = -3.0f;  // Dialogue-menu voice, player voice
		float weapons = 0.0f;   // Gunfire (2D 1st person and 3D), explosions, casings
		float footsteps = -6.0f;
		float loops3D = -6.0f;  // Placed ambient emitters
		float loops2D = kSendOff;
		float region = kSendOff;  // Region one-shots and ambience
		float default3D = 0.0f;
		float default2D = 0.0f;
	};

	// Filter for voices that the engine marks as `Modulated` (power armor helmets, masks, intercoms).
	struct MaskSettings {
		bool enabled = true;
		float gainDb = 3.0f;  // Loudness relative to the unfiltered voice
		float lowCutHz = 250.0f;
		float highCutHz = 3200.0f;
		float resonanceHz = 1000.0f;  // Air space inside the mask
		float resonanceGainDb = 5.0f;
		float driveDb = 3.0f;  // Speaker distortion, 0 = off
	};

	struct HotkeySettings {
		int bypassKey = 0x23;  // Virtual-key code (VK_END), 0 = no key
	};

	struct DebugSettings {
		// ANAM to use in all locations (0 = off).
		std::uint32_t forceEnvironment = 0;

		// Logs the slot and source state from OpenAL Soft for first n number of routed sounds.
		std::uint32_t readbackCount = 0;

		bool logSoundPlay = true;
		bool logSoundEnvironment = true;

		// Scan first n num of played sounds for DirectSound COM pointers.
		std::uint32_t layoutProbeCount = 32;

		// Sends the reverb and source properties of a sound as deferred sets.
		// The last set of each sound commits all of them at one time.
		bool deferEaxSets = false;

		// Logs the time spent in the reverb routing of each sound, as a summary every 500 sounds.
		bool logRouteTiming = false;
	};

	struct Settings {
		ReverbSettings reverb;
		SendSettings sends;
		MaskSettings mask;
		HotkeySettings hotkeys;
		DebugSettings debug;
	};

	// Reads the INI and logs the result.
	// Call one time, from `NVSEPlugin_Load`, before the hooks are installed.
	void Load(const std::string& iniPath);

	const Settings& Get();
}
