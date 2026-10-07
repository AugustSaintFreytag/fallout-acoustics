#pragma once

#include "effects/voice_cover.h"

namespace sea::debug {
	// Logs the cover of a speaking actor, the vanilla decision and the worn items on the head.
	void LogVoiceCover(void* actor, effects::VoiceCover cover, bool vanillaModulated);
}
