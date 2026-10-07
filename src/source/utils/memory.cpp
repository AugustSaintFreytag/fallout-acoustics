#include "utils/memory.h"

#include <Windows.h>
#include <Psapi.h>

#include <cstddef>

namespace sea::mem {
	bool SafeRead32(const void* address, std::uint32_t& out) {
		__try {
			out = *static_cast<const volatile std::uint32_t*>(address);

			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	void* PatchPointer(std::uintptr_t slot, void* replacement) {
		auto* target = reinterpret_cast<void**>(slot);
		DWORD oldProtection = 0;

		if (!VirtualProtect(target, sizeof(void*), PAGE_READWRITE, &oldProtection)) {
			return nullptr;
		}

		void* previous = *target;
		*target = replacement;
		VirtualProtect(target, sizeof(void*), oldProtection, &oldProtection);

		return previous;
	}

	bool PatchCall(std::uintptr_t callAddress, std::uintptr_t expectedTarget, void* replacement) {
		constexpr std::uint8_t kCallOpcode = 0xE8;
		constexpr std::size_t kCallSize = 5;

		auto* instruction = reinterpret_cast<std::uint8_t*>(callAddress);

		if (instruction[0] != kCallOpcode) {
			return false;
		}

		auto* relativeTarget = reinterpret_cast<std::int32_t*>(callAddress + 1);
		const std::uintptr_t nextInstruction = callAddress + kCallSize;

		if (nextInstruction + *relativeTarget != expectedTarget) {
			return false;
		}

		DWORD oldProtection = 0;

		if (!VirtualProtect(relativeTarget, sizeof(std::int32_t), PAGE_EXECUTE_READWRITE, &oldProtection)) {
			return false;
		}

		*relativeTarget = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(replacement) - nextInstruction);
		VirtualProtect(relativeTarget, sizeof(std::int32_t), oldProtection, &oldProtection);
		FlushInstructionCache(GetCurrentProcess(), instruction, kCallSize);

		return true;
	}

	ModuleRange GetModuleRange(const char* moduleName) {
		ModuleRange range;
		HMODULE module = GetModuleHandleA(moduleName);
		MODULEINFO moduleInfo{};

		if (!module || !GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo))) {
			return range;
		}

		range.begin = reinterpret_cast<std::uintptr_t>(moduleInfo.lpBaseOfDll);
		range.end = range.begin + moduleInfo.SizeOfImage;

		return range;
	}
}
