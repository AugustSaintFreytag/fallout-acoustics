#include "utils/memory.h"

#include <Windows.h>
#include <Psapi.h>

#include <cstddef>
#include <cstring>

namespace sea::mem {

	namespace {

		constexpr std::uint8_t kJumpOpcode = 0xE9;
		constexpr std::uint8_t kNopOpcode = 0x90;
		constexpr std::size_t kJumpSize = 5;

		constexpr std::size_t kTrampolinePoolSize = 4096;
		constexpr std::size_t kTrampolineAlignment = 16;

		// Thread: Main (load time only)
		std::uint8_t* g_trampolinePool = nullptr;
		std::size_t g_trampolinePoolUsed = 0;

		// Takes `size` bytes from the executable trampoline pool, 16-byte aligned. 
		// Returns `nullptr` if the pool is full.
		std::uint8_t* AllocateTrampoline(std::size_t size) {
			if (!g_trampolinePool) {
				g_trampolinePool = static_cast<std::uint8_t*>(
					VirtualAlloc(nullptr, kTrampolinePoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

				if (!g_trampolinePool) {
					return nullptr;
				}
			}

			const std::size_t alignedSize = (size + kTrampolineAlignment - 1) & ~(kTrampolineAlignment - 1);

			if (g_trampolinePoolUsed + alignedSize > kTrampolinePoolSize) {
				return nullptr;
			}

			std::uint8_t* trampoline = g_trampolinePool + g_trampolinePoolUsed;
			g_trampolinePoolUsed += alignedSize;

			return trampoline;
		}

		void WriteJump(std::uint8_t* instruction, std::uintptr_t target) {
			const std::uintptr_t nextInstruction = reinterpret_cast<std::uintptr_t>(instruction) + kJumpSize;

			const auto relativeTarget = static_cast<std::int32_t>(target - nextInstruction);

			instruction[0] = kJumpOpcode;
			std::memcpy(instruction + 1, &relativeTarget, sizeof(relativeTarget));
		}

	}

	// Reads 4 bytes from `address` into `out`. 
	// Returns false if the address can't be read.
	bool SafeRead32(const void* address, std::uint32_t& out) {
		__try {
			out = *static_cast<const volatile std::uint32_t*>(address);

			return true;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	// Replaces a pointer in read-only memory, like a vtable slot.
	// Returns the previous value, or `nullptr` if the replacement failed.
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

	// Redirects the `call rel32` instruction at `callAddress` to `replacement`.
	// Returns false if the instruction is not a call to `expectedTarget`, for example when another plugin patched it.
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

	// Replaces the first `length` bytes of a function with a jump to `hook`.
	// Returns a trampoline that runs the replaced bytes and continues the function.
	// Returns `nullptr` if the bytes differ from `expectedBytes`, for example when another plugin patched them.
	//
	// The bytes must be whole instructions without relative addresses, at least 5 bytes.
	//
	// Thread: Main (load time only)
	void* DetourFunction(std::uintptr_t address, const std::uint8_t* expectedBytes, std::size_t length, void* hook) {
		auto* function = reinterpret_cast<std::uint8_t*>(address);

		if (length < kJumpSize || std::memcmp(function, expectedBytes, length) != 0) {
			return nullptr;
		}

		std::uint8_t* trampoline = AllocateTrampoline(length + kJumpSize);

		if (!trampoline) {
			return nullptr;
		}

		std::memcpy(trampoline, function, length);
		WriteJump(trampoline + length, address + length);

		DWORD oldProtection = 0;

		if (!VirtualProtect(function, length, PAGE_EXECUTE_READWRITE, &oldProtection)) {
			return nullptr;
		}

		WriteJump(function, reinterpret_cast<std::uintptr_t>(hook));
		std::memset(function + kJumpSize, kNopOpcode, length - kJumpSize);

		VirtualProtect(function, length, oldProtection, &oldProtection);
		FlushInstructionCache(GetCurrentProcess(), function, length);

		return trampoline;
	}

	// Returns the file name of the module that contains `address`, or the string "?".
	// Parameter `buffer` points to the full module path.
	const char* ModuleNameAt(std::uintptr_t address, char* buffer, unsigned long size) {
		HMODULE module = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

		if (!GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(address), &module) || !GetModuleFileNameA(module, buffer, size)) {
			return "?";
		}

		const char* fileName = std::strrchr(buffer, '\\');

		if (!fileName) {
			return buffer;
		}

		return fileName + 1;
	}

	// Returns the address range of a loaded module. 
	// Calling with `nullptr` returns game executable. 
	// Empty if not loaded.
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
