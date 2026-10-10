#include "engine/holotapes.h"

#include "engine/addresses.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace sea::engine {

	using mem::Field;

	namespace {

		constexpr std::uint32_t kInvalidSoundId = 0xFFFFFFFF;

		// Limits the list walk in case the list is broken.
		constexpr std::size_t kMaxHolotapeSounds = 256;

		// Sound IDs in the holotape queue of the Pip-Boy.
		// Thread: Main (Write)
		// Thread: Audio (Read)
		std::mutex g_lock;
		std::vector<std::uint32_t> g_soundIds;

		// Thread: Main
		std::vector<std::uint32_t> g_lastSoundIds;

		// Returns the sound IDs of all lines queued for holotape playback, in play order.
		std::vector<std::uint32_t> ReadHolotapeSoundIds(void* mapMenu) {
			std::vector<std::uint32_t> soundIds;
			void* node = &Field<std::uint8_t>(mapMenu, kMapMenu_HolotapeSounds);

			while (node && soundIds.size() < kMaxHolotapeSounds) {
				const std::uint32_t soundId = Field<std::uint32_t>(node, kSoundHandle_ID);

				if (soundId != kInvalidSoundId) {
					soundIds.push_back(soundId);
				}

				node = Field<void*>(node, kSoundHandleNode_Next);
			}

			return soundIds;
		}

	}

	// Returns the Pip-Boy map menu, or null if it does not exist.
	//
	// Thread: Main
	void* GetMapMenu() {
		return mem::Global<void*>(kMapMenuSingleton);
	}

	// Copies the sound IDs of the Pip-Boy holotape queue for the audio thread.
	// The engine fills the queue when a holotape starts and plays the first line only after all lines are loaded.
	// So the copy is always ready before the first `Play`.
	//
	// Thread: Main (each frame)
	void UpdateHolotapeSounds() {
		std::vector<std::uint32_t> soundIds;
		void* mapMenu = GetMapMenu();

		if (mapMenu) {
			soundIds = ReadHolotapeSoundIds(mapMenu);
		}

		if (soundIds == g_lastSoundIds) {
			return;
		}

		g_lastSoundIds = soundIds;

		if (!soundIds.empty()) {
			SEA_LOG("[Holotape] Queue has %zu sound(s), first ID %u.", soundIds.size(), soundIds.front());
		}

		std::lock_guard guard(g_lock);
		g_soundIds = std::move(soundIds);
	}

	// Checks if the given sound is a line of the holotape that the Pip-Boy plays.
	//
	// Thread: Any
	bool IsHolotapeSound(void* gameSound) {
		const std::uint32_t soundId = Field<std::uint32_t>(gameSound, kSound_ID);

		std::lock_guard guard(g_lock);

		return std::find(g_soundIds.begin(), g_soundIds.end(), soundId) != g_soundIds.end();
	}

}
