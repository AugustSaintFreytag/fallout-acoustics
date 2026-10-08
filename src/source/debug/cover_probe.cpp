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

		// Writes worn items in head slots with their slots and power armor flag to given buffer.
		void DescribeHeadItems(void* actor, char* buffer, std::size_t size) {
			engine::ArmorMap worn[engine::kBipedAnim_SlotCount];
			const std::size_t count = engine::GetNumberOfEquippedItems(actor, worn, std::size(worn));

			std::snprintf(buffer, size, " none");
			std::size_t usedLength = 0;

			for (std::size_t index = 0; index < count; ++index) {
				const engine::ArmorMap& item = worn[index];

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

	// Logs cover of a speaking actor with the vanilla decision and worn head items.
	//
	// Thread: Any
	void LogVoiceCover(void* actor, effects::VoiceCover cover, bool vanillaModulated) {
		char headItems[512];
		DescribeHeadItems(actor, headItems, sizeof(headItems));

		SEA_LOG("[Cover] %08X '%s': %s (Vanilla Modulated=%d) Head items:%s", engine::GetFormID(actor),
			engine::GetEditorID(actor), effects::VoiceCoverName(cover), vanillaModulated, headItems);
	}

}
