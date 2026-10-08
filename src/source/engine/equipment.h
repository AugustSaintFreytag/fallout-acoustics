#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::engine {

	struct ArmorMap {
		void* armor;
		std::uint32_t slotMask;  // kBipedSlot_* bits
		bool isPowerArmor;
	};

	std::size_t GetNumberOfEquippedItems(void* actor, ArmorMap* armorMap, std::size_t capacity);

}
