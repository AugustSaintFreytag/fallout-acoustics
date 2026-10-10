#include "reverb/routing.h"

#include "config/settings.h"
#include "engine/sound_flags.h"
#include "engine/sound_paths.h"

namespace sea::reverb {

	// Checks if a sound is a radio placed in the world. The Pip-Boy radio plays in 2D without a radius.
	bool IsWorldRadio(std::uint32_t soundFlags) {
		using namespace engine;

		return (soundFlags & kSound_Radio) && (soundFlags & (kSound_3D | kSound_2DRadius));
	}

	// Picks the send category of a sound from its `SoundFlag` bits.
	Route Classify(std::uint32_t soundFlags) {
		using namespace engine;

		const config::SendLevels& sends = config::Get().spatialization.sends;

		// Other radio sounds, like the Pip-Boy radio, are excluded below.
		if (IsWorldRadio(soundFlags)) {
			return {"radio3D", sends.radio3D};
		}

		if (soundFlags & (kSound_SystemSound | kSound_Music | kSound_Radio)) {
			return {"excluded", config::kSendOff};
		}

		const bool is2D = !(soundFlags & kSound_3D);

		if (soundFlags & kSound_Voice) {
			if (is2D) {
				return {"voice2D", sends.voice2D};
			}

			return {"voice3D", sends.voice3D};
		}

		if (soundFlags & (kSound_2DGunfire | kSound_Battle)) {
			return {"weapons", sends.weapons};
		}

		if (soundFlags & kSound_Footsteps) {
			return {"footsteps", sends.footsteps};
		}

		if (soundFlags & kSound_Region) {
			return {"region", sends.region};
		}

		if (soundFlags & kSound_Loop) {
			if (is2D) {
				return {"loop2D", sends.loops2D};
			}

			return {"loop3D", sends.loops3D};
		}

		if (is2D) {
			return {"default2D", sends.default2D};
		}

		return {"default3D", sends.default3D};
	}

	// Returns the `[Sources]` level of a sound in dB, for dry and reverb alike. 0 = unchanged.
	// Ambience is a region sound or a sound in the ambience folder. Region flag on regional insects not verified.
	float GetSourceLevel(std::uint32_t soundFlags, const char* path) {
		const config::SourceSettings& sources = config::Get().sources;

		if (IsWorldRadio(soundFlags)) {
			return sources.radioLevel;
		}

		if ((soundFlags & engine::kSound_Region) || engine::IsAmbiencePath(path)) {
			return sources.ambienceLevel;
		}

		return 0.0f;
	}

}
