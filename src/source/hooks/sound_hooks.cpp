#include "hooks/sound_hooks.h"

#include "audio/directsound.h"
#include "config/settings.h"
#include "debug/occlusion_test.h"
#include "debug/request_probe.h"
#include "debug/route_timing.h"
#include "debug/sound_probe.h"
#include "debug/update_probe.h"
#include "effects/distance.h"
#include "effects/voice_filter.h"
#include "engine/addresses.h"
#include "occlusion/apply.h"
#include "reverb/reverb.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/threads.h"

#include <Windows.h>

#include <cstdint>

namespace sea::hooks {

	using mem::Field;

	namespace {

		using PlayFn = bool(__thiscall*)(void* sound, bool loop);
		using UpdateFn = bool(__thiscall*)(void* sound, DWORD timeDelta);
		using SetEnvironmentTypeFn = void(__thiscall*)(void* sound, std::uint32_t type);

		PlayFn g_originalPlay = nullptr;
		UpdateFn g_originalUpdate = nullptr;
		SetEnvironmentTypeFn g_originalSetEnvironmentType = nullptr;

		// Routes a sound into the reverb, sets its occlusion and filters a covered voice. Then plays it.
		bool __fastcall Hook_Play(void* sound, void* /*edx*/, bool loop) {
			threads::RememberAudioThread();
			effects::ScaleAttenuationDistances(sound);

			// Route before the original `Play` so first samples already reach the reverb.
			const std::int64_t routeStartTime = debug::BeginRouteTiming();
			const char* route = reverb::OnSoundPlay(sound);
			debug::EndRouteTiming(routeStartTime);

			occlusion::OnSoundPlay(sound);
			debug::ApplyTestOcclusionOnPlay(sound);
			debug::OnSoundStarted(sound, route);

			// Filter before the original `Play` so playback starts with filtered data.
			effects::ProcessVoiceFilter(sound);

			const bool result = g_originalPlay(sound, loop);

			debug::OnSoundPlayed(sound, loop, route);

			return result;
		}

		// Ramps occlusion of a playing sound and calls the original. Runs about every 2 ms per sound.
		bool __fastcall Hook_Update(void* sound, void* /*edx*/, DWORD timeDelta) {
			debug::OnSoundUpdate(sound);
			occlusion::OnSoundUpdate(sound);
			debug::ApplyTestOcclusionOnUpdate(sound);

			return g_originalUpdate(sound, timeDelta);
		}

		// Checks if any setting needs the `Update` hook. Only occlusion and its debug probes use it.
		bool NeedsUpdateHook() {
			const config::Settings& settings = config::Get();

			return settings.occlusion.enabled || settings.debug.probeSoundUpdate || settings.debug.testOcclusion != 0;
		}

		// Logs the engine's own environment changes on a sound (`[Debug] bLogSoundEnvironment`).
		void __fastcall Hook_SetEnvironmentType(void* sound, void* /*edx*/, std::uint32_t type) {
			const std::uint32_t previous = Field<std::uint32_t>(sound, engine::kSound_EnvironmentType);
			g_originalSetEnvironmentType(sound, type);

			debug::OnEnvironmentTypeChanged(sound, previous, type);
		}

		void* PatchSoundVtable(std::uintptr_t slotOffset, void* hook) {
			return mem::PatchPointer(engine::kVtbl_BSWin32GameSound + slotOffset, hook);
		}

	}

	// Patches `Play` in the `BSWin32GameSound` vtable. Also patches `Update` and `SetEnvironmentType` where settings need them.
	// Returns false if a patch failed.
	//
	// Called once from `NVSEPlugin_Load` after `config::Load`.
	//
	// Thread: Main (load time only)
	bool InstallSoundHooks() {
		g_originalPlay = static_cast<PlayFn>(PatchSoundVtable(engine::kSoundVtbl_Play, reinterpret_cast<void*>(&Hook_Play)));

		SEA_LOG("Hooks: BSWin32GameSound::Play %p -> %p, dsound.dll %08X..%08X", g_originalPlay, &Hook_Play,
			static_cast<unsigned>(audio::DSoundBegin()), static_cast<unsigned>(audio::DSoundEnd()));

		if (!g_originalPlay) {
			return false;
		}

		if (NeedsUpdateHook()) {
			g_originalUpdate = static_cast<UpdateFn>(PatchSoundVtable(engine::kSoundVtbl_Update, reinterpret_cast<void*>(&Hook_Update)));

			SEA_LOG("Hooks: BSWin32GameSound::Update %p -> %p", g_originalUpdate, &Hook_Update);

			if (!g_originalUpdate) {
				return false;
			}
		}

		// The `SetEnvironmentType` hook only logs. Install it only for `[Debug] bLogSoundEnvironment`.
		if (!config::Get().debug.logSoundEnvironment) {
			return true;
		}

		g_originalSetEnvironmentType = static_cast<SetEnvironmentTypeFn>(
			PatchSoundVtable(engine::kSoundVtbl_SetEnvironmentType, reinterpret_cast<void*>(&Hook_SetEnvironmentType)));

		SEA_LOG("Hooks: BSWin32GameSound::SetEnvironmentType %p -> %p", g_originalSetEnvironmentType, &Hook_SetEnvironmentType);

		return g_originalSetEnvironmentType != nullptr;
	}

}
