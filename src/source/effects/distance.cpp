#include "effects/distance.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/sound_flags.h"
#include "utils/memory.h"

#include <array>
#include <cstdint>

namespace sea::effects {

	using mem::Field;

	namespace {

		// Thread: Audio
		std::array<std::uint32_t, 256> g_scaledSoundIds{};
		std::size_t g_nextScaledIndex = 0;

		bool IsScaledFactor() {
			return config::Get().sources.attenuationFactor != 1.0f;
		}

		bool HasDistanceAttenuation(void* sound) {
			const std::uint32_t soundFlags = Field<std::uint32_t>(sound, engine::kSound_TypeFlags);

			return (soundFlags & (engine::kSound_3D | engine::kSound_2DRadius)) != 0;
		}

		// Checks if the given sound was scaled before. `Play` can run more than once for one sound.
		bool WasScaled(std::uint32_t soundId) {
			for (const std::uint32_t scaledId : g_scaledSoundIds) {
				if (scaledId == soundId) {
					return true;
				}
			}

			return false;
		}

		void RememberScaled(std::uint32_t soundId) {
			g_scaledSoundIds[g_nextScaledIndex] = soundId;
			g_nextScaledIndex = (g_nextScaledIndex + 1) % g_scaledSoundIds.size();
		}

	}

	// Multiplies the min and max attenuation distance of a 3D sound by `[Sources] fDistanceAttenuationFactor`.
	// Called before the original `Play`. Scales each sound once.
	//
	// Thread: Audio
	void ScaleAttenuationDistances(void* sound) {
		if (!IsScaledFactor() || !HasDistanceAttenuation(sound)) {
			return;
		}

		const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);

		if (WasScaled(soundId)) {
			return;
		}

		RememberScaled(soundId);

		const float factor = config::Get().sources.attenuationFactor;
		Field<float>(sound, engine::kSound_MinAttenuationDistance) *= factor;
		Field<float>(sound, engine::kSound_MaxAttenuationDistance) *= factor;
	}

}
