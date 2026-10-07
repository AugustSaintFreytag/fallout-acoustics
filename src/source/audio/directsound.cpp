#include "audio/directsound.h"

#include "utils/memory.h"

namespace sea::audio {
	namespace {
		const mem::ModuleRange& DSoundRange() {
			static const mem::ModuleRange range = mem::GetModuleRange("dsound.dll");

			return range;
		}
	}

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

	bool Supports(IUnknown* object, REFIID iid) {
		IUnknown* result = nullptr;

		if (FAILED(object->QueryInterface(iid, reinterpret_cast<void**>(&result))) || !result) {
			return false;
		}

		result->Release();

		return true;
	}

	std::uintptr_t DSoundBegin() {
		return DSoundRange().begin;
	}

	std::uintptr_t DSoundEnd() {
		return DSoundRange().end;
	}
}
