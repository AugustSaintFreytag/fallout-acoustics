#include "effects/voice_cover.h"

#include "engine/addresses.h"
#include "engine/equipment.h"
#include "engine/sound_flags.h"

#include <iterator>

namespace sea::effects {

	namespace {

		// Returns the cover of one worn item, by its biped slots.
		VoiceCover ClassifyArmor(const engine::ArmorMap& armor) {
			if (armor.slotMask & engine::kBipedSlot_Head) {
				if (armor.isPowerArmor) {
					return VoiceCover::Speaker;
				}

				return VoiceCover::Full;
			}

			if (armor.slotMask & engine::kBipedSlot_Mask) {
				if (armor.slotMask & engine::kBipedSlot_Hair) {
					return VoiceCover::Full;
				}

				return VoiceCover::Light;
			}

			return VoiceCover::None;
		}

	}

	// Returns a designation for the given voice cover.
	const char* VoiceCoverName(VoiceCover cover) {
		switch (cover) {
		case VoiceCover::Light:
			return "Light";

		case VoiceCover::Full:
			return "Full";

		case VoiceCover::Speaker:
			return "Speaker";

		default:
			return "None";
		}
	}

	// Returns the cover of a speaking actor, from its worn armor. The item with the most cover counts:
	// Head slot = Speaker with the power armor flag, else Full. Mask and Hair slots = Full. Mask slot only = Light.
	VoiceCover ClassifySpeaker(void* actor) {
		engine::ArmorMap worn[engine::kBipedAnim_SlotCount];
		const std::size_t count = engine::GetNumberOfEquippedItems(actor, worn, std::size(worn));

		VoiceCover cover = VoiceCover::None;

		for (std::size_t index = 0; index < count; ++index) {
			const VoiceCover itemCover = ClassifyArmor(worn[index]);

			if (itemCover > cover) {
				cover = itemCover;
			}
		}

		return cover;
	}

	// Returns the sound flags that tag a voice with its cover. Speaker uses the engine's own `Modulated` flag.
	std::uint32_t VoiceCoverFlags(VoiceCover cover) {
		switch (cover) {
		case VoiceCover::Light:
			return engine::kSound_CoverLight;

		case VoiceCover::Full:
			return engine::kSound_CoverFull;

		case VoiceCover::Speaker:
			return engine::kSound_Modulated;

		default:
			return 0;
		}
	}

	// Returns the cover of a voice from its sound flags. `Modulated` set by the engine (for example, intercoms) is Speaker.
	VoiceCover VoiceCoverFromFlags(std::uint32_t soundFlags) {
		if (soundFlags & engine::kSound_Modulated) {
			return VoiceCover::Speaker;
		}

		if (soundFlags & engine::kSound_CoverFull) {
			return VoiceCover::Full;
		}

		if (soundFlags & engine::kSound_CoverLight) {
			return VoiceCover::Light;
		}

		return VoiceCover::None;
	}

}
