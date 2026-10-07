#include "effects/voice_cover.h"

#include "engine/addresses.h"
#include "engine/equipment.h"
#include "engine/sound_flags.h"

#include <iterator>

namespace sea::effects {
	namespace {
		VoiceCover ClassifyArmor(const engine::WornArmor& armor) {
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

	VoiceCover ClassifySpeaker(void* actor) {
		engine::WornArmor worn[engine::kBipedAnim_SlotCount];
		const std::size_t count = engine::GetWornArmor(actor, worn, std::size(worn));

		VoiceCover cover = VoiceCover::None;

		for (std::size_t index = 0; index < count; ++index) {
			const VoiceCover itemCover = ClassifyArmor(worn[index]);

			if (itemCover > cover) {
				cover = itemCover;
			}
		}

		return cover;
	}

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
