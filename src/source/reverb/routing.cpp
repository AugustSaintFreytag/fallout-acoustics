#include "reverb/routing.h"

#include "config/settings.h"
#include "engine/sound_flags.h"

namespace sea::reverb {
	Route Classify(std::uint32_t soundFlags) {
		using namespace engine;

		const config::SendSettings& sends = config::Get().sends;

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
}
