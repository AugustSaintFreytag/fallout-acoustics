#pragma once

namespace sea::effects {
	// Audio thread. Call before the original Play.
	// Filters the buffer of a covered voice in place, with the preset of its cover.
	// DSOAL only: native DirectSound applies the vanilla effect.
	void ProcessVoiceFilter(void* gameSound);
}
