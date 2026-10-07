#pragma once

namespace sea::reverb {

	// One global EAX reverb in FX slot 0. Current player acoustic space sets environment.
	// Each sound sends into it at the level of its category.
	//
	// Threading: Main thread only writes atomics.
	// Audio thread handles all EAX calls, done in `BSWin32GameSound::Play`,
	// using property set of buffer that initiated playback.

	// Main thread. When bypass is enabled, wet output is muted.
	// Returns true if processing is active (not bypassed/disabled) after the toggle.
	bool ToggleBypass();

	// Audio thread. Call before the original Play.
	// Returns the routing label for the log.
	const char* OnSoundPlay(void* gameSound);
}
