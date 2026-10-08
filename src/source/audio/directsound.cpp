#include "audio/directsound.h"

#include "utils/memory.h"

namespace sea::audio {

	namespace {
		const mem::ModuleRange& DSoundRange() {
			static const mem::ModuleRange range = mem::GetModuleRange("dsound.dll");

			return range;
		}
	}

	// Checks if given address holds a DirectSound object. Its vtable must lie in `dsound.dll`.
	// Returns true for DirectSound and DSOAL objects. Safe for invalid addresses.
	bool IsDSoundObject(std::uint32_t address) {
		if (address == 0) {
			return false;
		}

		std::uint32_t vtable = 0;

		if (!mem::SafeRead32(reinterpret_cast<void*>(address), vtable)) {
			return false;
		}

		return DSoundRange().Contains(vtable);
	}

	// Checks if given object implements given interface. Releases the queried interface again.
	bool Supports(IUnknown* object, REFIID iid) {
		IUnknown* result = nullptr;

		if (FAILED(object->QueryInterface(iid, reinterpret_cast<void**>(&result))) || !result) {
			return false;
		}

		result->Release();

		return true;
	}

	// Checks if DSOAL replaces DirectSound by looking for its OpenAL driver `dsoal-aldrv.dll`.
	bool IsDSOALLoaded() {
		return GetModuleHandleA("dsoal-aldrv.dll") != nullptr;
	}

	// Returns start of the `dsound.dll` address range or 0 if not loaded.
	std::uintptr_t DSoundBegin() {
		return DSoundRange().begin;
	}

	std::uintptr_t DSoundEnd() {
		return DSoundRange().end;
	}

}
