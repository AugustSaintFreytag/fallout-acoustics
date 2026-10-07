#pragma once
#include <cstdint>

namespace sea::mem {
	// Raw engine object property, careful.
	template <typename T>
	inline T& Field(void* base, std::uintptr_t offset) {
		return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(base) + offset);
	}

	template <typename T>
	inline T Global(std::uintptr_t address) {
		return *reinterpret_cast<T*>(address);
	}

	// Reads 4 bytes and returns false when address can't be read.
	bool SafeRead32(const void* address, std::uint32_t& out);

	// Replaces a pointer in read-only memory, like a vtable slot.
	// Returns previous value, or `nullptr` if write fails.
	void* PatchPointer(std::uintptr_t slot, void* replacement);

	// Redirects a `call rel32` instruction at `callAddress` to `replacement`.
	// Returns false if the instruction is not a call to `expectedTarget` (for example, another plugin patched it).
	bool PatchCall(std::uintptr_t callAddress, std::uintptr_t expectedTarget, void* replacement);

	struct ModuleRange {
		std::uintptr_t begin = 0;
		std::uintptr_t end = 0;
		bool Contains(std::uintptr_t address) const {
			return address >= begin && address < end;
		}
	};

	// `nullptr` returns main process.
	ModuleRange GetModuleRange(const char* moduleName);
}
