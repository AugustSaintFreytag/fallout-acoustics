#pragma once

namespace sea::effects {
	// Audio thread. Call before the original Play.
	// Filters the buffer of a `Modulated` sound in place (DSOAL only, native DirectSound applies the vanilla effect).
	void ProcessModulatedVoice(void* gameSound);
}
