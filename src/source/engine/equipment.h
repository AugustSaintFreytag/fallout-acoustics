#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::engine {
	struct WornArmor {
		void* armor;
		std::uint32_t slotMask;  // kBipedSlot_* bits
		bool isPowerArmor;
	};

	// Fills `worn` with each different armor item that a character shows.
	// Returns the count. Returns 0 for creatures and for characters without loaded 3D.
	std::size_t GetWornArmor(void* actor, WornArmor* worn, std::size_t capacity);
}
