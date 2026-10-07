#include "engine/equipment.h"

#include "engine/addresses.h"
#include "utils/memory.h"

namespace sea::engine {
	using mem::Field;

	namespace {
		// An item that covers more than one slot shows in each of its slots.
		bool Contains(const WornArmor* worn, std::size_t count, void* armor) {
			for (std::size_t index = 0; index < count; ++index) {
				if (worn[index].armor == armor) {
					return true;
				}
			}

			return false;
		}
	}

	std::size_t GetWornArmor(void* actor, WornArmor* worn, std::size_t capacity) {
		if (!actor || Field<std::uint8_t>(actor, kForm_TypeID) != kFormType_Character) {
			return 0;
		}

		void* bipedAnim = Field<void*>(actor, kCharacter_BipedAnim);

		if (!bipedAnim) {
			return 0;
		}

		std::size_t count = 0;

		for (std::uint32_t slot = 0; slot < kBipedAnim_SlotCount && count < capacity; ++slot) {
			void* item = Field<void*>(bipedAnim, kBipedAnim_SlotData + slot * kBipedAnim_SlotSize);

			// An empty slot can hold the race instead of an item.
			if (!item || Field<std::uint8_t>(item, kForm_TypeID) != kFormType_TESObjectARMO) {
				continue;
			}

			if (Contains(worn, count, item)) {
				continue;
			}

			const std::uint32_t bipedFlags = Field<std::uint32_t>(item, kArmor_BipedFlags);

			worn[count] = {item, Field<std::uint32_t>(item, kArmor_SlotMask), (bipedFlags & kBipedFlag_PowerArmor) != 0};
			++count;
		}

		return count;
	}
}
