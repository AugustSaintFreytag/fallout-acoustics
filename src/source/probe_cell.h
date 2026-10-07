#pragma once

namespace sea::probe {
	// Call once every main game loop (per frame). 
	void PollPlayerAcoustics();

	// Clear last state.
	void ResetPlayerAcoustics();
}
