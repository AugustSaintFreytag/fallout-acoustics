#include "fixes/load_holotape_sounds.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/sound_paths.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/timing.h"

#include <atomic>
#include <cstdint>

namespace sea::fixes {

	using mem::Field;

	namespace {

		using SetVolumeFn = bool(__thiscall*)(void* sound, float volume);

		// Milliseconds after `PostLoadGame` in which the holotape sounds stay muted.
		constexpr double kMuteWindowMs = 1000.0;

		// Thread: Main (Write), Audio (Read)
		std::atomic<bool> g_loading = false;
		std::atomic<std::int64_t> g_loadEndTime = 0;

		bool IsInMuteWindow() {
			if (g_loading.load(std::memory_order_acquire)) {
				return true;
			}

			const std::int64_t loadEndTime = g_loadEndTime.load(std::memory_order_acquire);

			if (loadEndTime == 0) {
				return false;
			}

			return timing::TicksToMilliseconds(static_cast<double>(timing::Now() - loadEndTime)) < kMuteWindowMs;
		}

	}

	// Opens the mute window when a save starts loading.
	//
	// Thread: Main
	void OnPreLoadGame() {
		g_loading.store(true, std::memory_order_release);
	}

	// Starts the timed end of the mute window when a save has loaded.
	//
	// Thread: Main
	void OnPostLoadGame() {
		g_loadEndTime.store(timing::Now(), std::memory_order_release);
		g_loading.store(false, std::memory_order_release);
	}

	// Sets the volume of the given sound to 0 if it is a Pip-Boy holotape start or stop sound
	// that plays while a save loads or in the first second after.
	// Needs `[Fixes] bFixLoadHolotapeSounds`. Must run before the original `Play`.
	//
	// The engine plays these sounds on some loads, with or without a holotape playing in the Pip-Boy.
	// The engine still plays and frees the muted sound as usual.
	//
	// Thread: Audio
	void MuteLoadHolotapeSound(void* sound) {
		if (!config::Get().fixes.loadHolotapeSounds) {
			return;
		}

		if (!IsInMuteWindow()) {
			return;
		}

		const char* path = &Field<char>(sound, engine::kSound_FilePath);

		if (!engine::IsHolotapeStartStopPath(path)) {
			return;
		}

		const auto setVolume = Field<SetVolumeFn>(Field<void*>(sound, 0), engine::kSoundVtbl_SetVolume);
		setVolume(sound, 0.0f);

		SEA_LOG("[Holotape] Muted '%s' on game load.", path);
	}

}
