#include "engine/equipment.h"

#include "engine/addresses.h"
#include "utils/memory.h"

namespace sea::engine {

	using mem::Field;

	namespace {
		// Checks if the given armor exists within the first `count` entries of the provided worn armor map.
		bool Contains(const ArmorMap* armorMap, std::size_t count, void* armor) {
			for (std::size_t index = 0; index < count; ++index) {
				if (armorMap[index].armor == armor) {
					return true;
				}
			}

			return false;
		}
	}

	// Evaluates the given armor map and populates it with flag if worn by the supplied actor.
	// Returns the number of matching items worn. 
	// Returns 0 for invalid actors.
	std::size_t GetNumberOfEquippedItems(void* actor, ArmorMap* armorMap, std::size_t capacity) {
		if (!actor || Field<std::uint8_t>(actor, kForm_TypeID) != kFormType_Character) {
			return 0;
		}

		void* bipedAnim = Field<void*>(actor, kCharacter_BipedAnim);

		if (!bipedAnim) {
			return 0;
		}

		std::size_t index = 0;

		for (std::uint32_t slot = 0; slot < kBipedAnim_SlotCount && index < capacity; ++slot) {
			void* item = Field<void*>(bipedAnim, kBipedAnim_SlotData + slot * kBipedAnim_SlotSize);

			// An empty slot can hold the race instead of an item.
			if (!item || Field<std::uint8_t>(item, kForm_TypeID) != kFormType_TESObjectARMO) {
				continue;
			}

			if (Contains(armorMap, index, item)) {
				continue;
			}

			const std::uint32_t bipedFlags = Field<std::uint32_t>(item, kArmor_BipedFlags);

			armorMap[index] = {item, Field<std::uint32_t>(item, kArmor_SlotMask), (bipedFlags & kBipedFlag_PowerArmor) != 0};
			++index;
		}

		return index;
	}

}
