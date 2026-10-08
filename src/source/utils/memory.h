#pragma once
#include <cstddef>
#include <cstdint>

namespace sea::mem {
	// Returns a reference to the field at `offset` bytes into an engine object.
	template <typename T>
	inline T& Field(void* base, std::uintptr_t offset) {
		return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(base) + offset);
	}

	// Returns the value at a fixed address. Works like an engine singleton pointer.
	template <typename T>
	inline T Global(std::uintptr_t address) {
		return *reinterpret_cast<T*>(address);
	}

	bool SafeRead32(const void* address, std::uint32_t& out);

	void* PatchPointer(std::uintptr_t slot, void* replacement);

	bool PatchCall(std::uintptr_t callAddress, std::uintptr_t expectedTarget, void* replacement);

	void* DetourFunction(std::uintptr_t address, const std::uint8_t* expectedBytes, std::size_t length, void* hook);

	struct ModuleRange {
		std::uintptr_t begin = 0;
		std::uintptr_t end = 0;
		bool Contains(std::uintptr_t address) const {
			return address >= begin && address < end;
		}
	};

	ModuleRange GetModuleRange(const char* moduleName);

	const char* ModuleNameAt(std::uintptr_t address, char* buffer, unsigned long size);
}
