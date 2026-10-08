#include "hooks/havok_hooks.h"

#include "config/settings.h"
#include "debug/pick_census.h"
#include "engine/addresses.h"
#include "engine/havok.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <intrin.h>

#include <cstdint>

namespace sea::hooks {

	namespace {

		using PickObjectFn = bool(__thiscall*)(void* world, void* pickData);

		// One original per vtable. Another plugin may have patched only one of them.
		PickObjectFn g_originalWorldPick = nullptr;
		PickObjectFn g_originalWorldMPick = nullptr;

		// Counts an engine pick for the census and calls the original. Skips the plugin's own casts.
		//
		// `_ReturnAddress` must be read in the hook itself.
		bool __fastcall Hook_WorldPick(void* world, void* /*edx*/, void* pickData) {
			if (!engine::IsInsidePluginCast()) {
				debug::OnEnginePick(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
			}

			return g_originalWorldPick(world, pickData);
		}

		bool __fastcall Hook_WorldMPick(void* world, void* /*edx*/, void* pickData) {
			if (!engine::IsInsidePluginCast()) {
				debug::OnEnginePick(reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
			}

			return g_originalWorldMPick(world, pickData);
		}

		// Points the pick slot of a vtable at given hook and logs it. Returns the original function.
		PickObjectFn PatchPickSlot(std::uintptr_t vtable, void* hook, const char* name) {
			auto* original = static_cast<PickObjectFn>(mem::PatchPointer(vtable + engine::kWorldVtbl_PickObject, hook));

			SEA_LOG("Hooks: %s::PickObject %p -> %p", name, original, hook);

			return original;
		}

	}

	// Patches `bhkWorld::PickObject` in the `bhkWorld` and `bhkWorldM` vtables if `[Debug] bProbePickThreads` is on.
	// Pick census then counts each engine cast by thread and caller. Returns false if a patch failed.
	//
	// Thread: Main (load time only)
	bool InstallHavokHooks() {
		if (!config::Get().debug.probePickThreads) {
			return true;
		}

		g_originalWorldPick = PatchPickSlot(engine::kVtbl_bhkWorld, reinterpret_cast<void*>(&Hook_WorldPick), "bhkWorld");
		g_originalWorldMPick = PatchPickSlot(engine::kVtbl_bhkWorldM, reinterpret_cast<void*>(&Hook_WorldMPick), "bhkWorldM");

		return g_originalWorldPick && g_originalWorldMPick;
	}

}
