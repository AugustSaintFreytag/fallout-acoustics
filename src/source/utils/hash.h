#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::hash {

	// Returns the FNV-1a hash (64 bit) of a block of memory.
	inline std::uint64_t Fnv1a(const void* data, std::size_t size) {
		const auto* bytes = static_cast<const std::uint8_t*>(data);
		std::uint64_t hash = 0xCBF29CE484222325ull;

		for (std::size_t index = 0; index < size; ++index) {
			hash ^= bytes[index];
			hash *= 0x100000001B3ull;
		}

		return hash;
	}

}
