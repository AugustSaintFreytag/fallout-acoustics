#pragma once

#include <cstdint>

namespace sea::reverb {
		
	// One global EAX reverb in FX slot 0. Current player acoustic space sets environment.
	// Each sound sends into it at the level of its category.
	//
	// Threading: Main thread only writes atomics.
	// Audio thread handles all EAX calls, done in `BSWin32GameSound::Play`,
	// using property set of buffer that initiated playback.

	constexpr float kSendOff = -100.0f;  // dB. A send at or below this level is considered off/muted.

	struct Config {
		bool enabled = true;
		float wetLevelDb = 0.0f;  // FX slot 0 volume
		std::uint32_t interiorFallback = 26;  // ANAM for an interior without an acoustic space (MediumRoom)
		std::uint32_t exteriorFallback = 18;  // ANAM for an exterior without an acoustic space (City)

		// Send level of each sound category, in dB.

		float sendVoice3D = 0.0f;
		float sendVoice2D = -3.0f;  // Dialogue-menu voice, player voice
		float sendWeapons = 0.0f;   // Gunfire (2D 1st person and 3D), explosions, casings
		float sendFootsteps = -6.0f;
		float sendLoops3D = -6.0f;  // Placed ambient emitters
		float sendLoops2D = kSendOff;
		float sendRegion = kSendOff;  // Region one-shots and ambience
		float sendDefault3D = 0.0f;
		float sendDefault2D = 0.0f;

		// dB, added to the room level of each preset. 
		// Presets are at -10 dB. Result is limited to 0 dB.
		float roomBoostDb = 0.0f;

		// Debug: ANAM to use in all locations (0 = off).
		std::uint32_t forceEnvironment = 0;
		// Debug: Logs the slot and source state from OpenAL Soft for first n number of routed sounds.
		std::uint32_t readbackCount = 0;
	};

	void Configure(const Config& config);
	const Config& GetConfig();

	// Main thread, run once on every frame.
	void UpdateListenerEnvironment();

	// Main thread. When bypass is enabled, wet output is muted. 
	// Returns true if processing is active (not bypassed/disabled) after the toggle.
	bool ToggleBypass();

	// Audio thread. Call before the original Play. 
	// Returns the routing label for the log.
	const char* OnSoundPlay(void* gameSound);
}
