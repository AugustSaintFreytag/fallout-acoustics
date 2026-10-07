#include "debug/cover_probe.h"

#include "engine/addresses.h"
#include "engine/equipment.h"
#include "engine/objects.h"
#include "utils/log.h"

#include <cstdio>
#include <iterator>

namespace sea::debug {
	namespace {
		constexpr std::uint32_t kHeadSlots = engine::kBipedSlot_Head | engine::kBipedSlot_Hair | engine::kBipedSlot_Headband
			| engine::kBipedSlot_Hat | engine::kBipedSlot_Eyeglasses | engine::kBipedSlot_Mask | engine::kBipedSlot_MouthObject;

		void DescribeHeadItems(void* actor, char* buffer, std::size_t size) {
			engine::WornArmor worn[engine::kBipedAnim_SlotCount];
			const std::size_t count = engine::GetWornArmor(actor, worn, std::size(worn));

			std::snprintf(buffer, size, " none");
			std::size_t usedLength = 0;

			for (std::size_t index = 0; index < count; ++index) {
				const engine::WornArmor& item = worn[index];

				if (!(item.slotMask & kHeadSlots)) {
					continue;
				}

				const int written = std::snprintf(buffer + usedLength, size - usedLength, " %08X '%s' Slots=%05X PA=%d;",
					engine::GetFormID(item.armor), engine::GetEditorID(item.armor), item.slotMask, item.isPowerArmor);

				if (written < 0 || usedLength + written >= size) {
					return;
				}

				usedLength += written;
			}
		}
	}

	void LogVoiceCover(void* actor, effects::VoiceCover cover, bool vanillaModulated) {
		char headItems[512];
		DescribeHeadItems(actor, headItems, sizeof(headItems));

		SEA_LOG("[Cover] %08X '%s': %s (Vanilla Modulated=%d) Head items:%s", engine::GetFormID(actor),
			engine::GetEditorID(actor), effects::VoiceCoverName(cover), vanillaModulated, headItems);
	}
}
