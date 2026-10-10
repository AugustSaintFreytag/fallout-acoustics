#include "fixes/holotape_duration.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/holotapes.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <algorithm>

namespace sea::fixes {

	using mem::Field;

	namespace {

		// Shortest total time to write, in ms. 0 stops the engine's time display.
		constexpr float kMinTotalTime = 1.0f;

		// Total time this fix wrote last, in ms. 0 = none.
		// Thread: Main
		float g_writtenTotalTime = 0.0f;

	}

	// Adds `[Fixes] fHolotapeDurationOffset` to the total time of the playing holotape.
	// The engine sets the total once, when all lines are loaded. A total that differs from the one written last is new.
	// The total only drives the time and progress display in the Pip-Boy. Playback is not changed.
	//
	// Thread: Main (each frame)
	void UpdateHolotapeDuration() {
		const float offsetMs = config::Get().fixes.holotapeDurationOffset * 1000.0f;
		void* mapMenu = engine::GetMapMenu();

		if (offsetMs == 0.0f || !mapMenu) {
			return;
		}

		float& totalTime = Field<float>(mapMenu, engine::kMapMenu_HolotapeTotalTime);

		if (totalTime <= 0.0f) {
			g_writtenTotalTime = 0.0f;

			return;
		}

		if (totalTime == g_writtenTotalTime) {
			return;
		}

		const float engineTotalTime = totalTime;
		totalTime = std::max(engineTotalTime + offsetMs, kMinTotalTime);
		g_writtenTotalTime = totalTime;

		SEA_LOG("[Holotape] Total time %.1f s -> %.1f s", engineTotalTime / 1000.0f, totalTime / 1000.0f);
	}

}
