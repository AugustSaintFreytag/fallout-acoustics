#pragma once

#include <cstdint>

namespace sea::effects {
	// What a voice goes through before it reaches the room. The order is the priority.
	enum class VoiceCover : std::uint8_t {
		None,     // Unobstructed
		Light,    // Cloth masks, semi-open helmets
		Full,     // Gas masks, closed helmets
		Speaker,  // Power armor helmets, intercoms
	};

	const char* VoiceCoverName(VoiceCover cover);

	// Cover of a speaking actor, from its worn armor. The item with the most cover counts:
	// Head slot = Speaker with the power armor flag, else Full. Mask and Hair slots = Full. Mask slot only = Light.
	VoiceCover ClassifySpeaker(void* actor);

	// Sound flags that tag a voice with its cover.
	std::uint32_t VoiceCoverFlags(VoiceCover cover);

	// Reads the cover tag. `Modulated` without our tag (for example, intercoms) is Speaker.
	VoiceCover VoiceCoverFromFlags(std::uint32_t soundFlags);
}
