#include "utils/memory.h"

#include <Windows.h>
#include <Psapi.h>

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
