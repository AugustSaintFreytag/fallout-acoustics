#include "hooks/sound_hooks.h"

#include "audio/directsound.h"
#include "config/settings.h"
#include "debug/sound_probe.h"
#include "engine/addresses.h"
#include "reverb/reverb.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstdint>

namespace sea::hooks {
	using mem::Field;

	namespace {
		using PlayFn = bool(__thiscall*)(void* sound, bool loop);
		using SetEnvironmentTypeFn = void(__thiscall*)(void* sound, std::uint32_t type);

		PlayFn g_originalPlay = nullptr;
		SetEnvironmentTypeFn g_originalSetEnvironmentType = nullptr;

		bool __fastcall Hook_Play(void* sound, void* /*edx*/, bool loop) {
			// Route before the original `Play` call, let first samples go to reverb immediately.
			const char* route = reverb::OnSoundPlay(sound);
			const bool result = g_originalPlay(sound, loop);

			debug::OnSoundPlayed(sound, loop, route);

			return result;
		}

		void __fastcall Hook_SetEnvironmentType(void* sound, void* /*edx*/, std::uint32_t type) {
			const std::uint32_t previous = Field<std::uint32_t>(sound, engine::kSound_EnvironmentType);
			g_originalSetEnvironmentType(sound, type);

			debug::OnEnvironmentTypeChanged(sound, previous, type);
		}

		void* PatchSoundVtable(std::uintptr_t slotOffset, void* hook) {
			return mem::PatchPointer(engine::kVtbl_BSWin32GameSound + slotOffset, hook);
		}
	}

	bool InstallSoundHooks() {
		g_originalPlay = static_cast<PlayFn>(PatchSoundVtable(engine::kSoundVtbl_Play, reinterpret_cast<void*>(&Hook_Play)));

		SEA_LOG("Hooks: BSWin32GameSound::Play %p -> %p, dsound.dll %08X..%08X", g_originalPlay, &Hook_Play,
			static_cast<unsigned>(audio::DSoundBegin()), static_cast<unsigned>(audio::DSoundEnd()));

		if (!g_originalPlay) {
			return false;
		}

		// The SetEnvironmentType hook only logs, so it is installed only for `[Debug] bLogSoundEnvironment`.
		if (!config::Get().debug.logSoundEnvironment) {
			return true;
		}

		g_originalSetEnvironmentType = static_cast<SetEnvironmentTypeFn>(
			PatchSoundVtable(engine::kSoundVtbl_SetEnvironmentType, reinterpret_cast<void*>(&Hook_SetEnvironmentType)));

		SEA_LOG("Hooks: BSWin32GameSound::SetEnvironmentType %p -> %p", g_originalSetEnvironmentType, &Hook_SetEnvironmentType);

		return g_originalSetEnvironmentType != nullptr;
	}
}
