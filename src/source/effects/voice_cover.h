#pragma once

#include <cstdint>

namespace sea::effects {

	// A kind of face cover that a voice passes through before it reaches the room.
	// A later value has priority over an earlier one.
	enum class VoiceCover : std::uint8_t {
		None,     // Unobstructed
		Light,    // Cloth masks, semi-open helmets
		Full,     // Gas masks, closed helmets
		Speaker,  // Power armor helmets, intercoms
	};

	const char* VoiceCoverName(VoiceCover cover);

	VoiceCover ClassifySpeaker(void* actor);

	std::uint32_t VoiceCoverFlags(VoiceCover cover);

	VoiceCover VoiceCoverFromFlags(std::uint32_t soundFlags);

}
