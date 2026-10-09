#include "reverb/listener.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/environment.h"
#include "engine/objects.h"
#include "engine/scene.h"
#include "reverb/presets.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <atomic>
#include <mutex>

namespace sea::reverb {

	using mem::Field;

	namespace {

		// Thread: Main (Read, Write)
		// Thread: Audio (Read)
		std::atomic<std::uint32_t> g_listenerEnvironment{0};  // ANAM, 0 = unknown

		// Thread: Main (Read, Write)
		// Thread: Audio (Read)
		std::atomic<bool> g_listenerInExterior{false};

		// Camera position. Game units.
		// Thread: Main (Write)
		// Thread: Audio (Read)
		std::mutex g_positionLock;
		Vector3 g_listenerPosition;

		void UpdateListenerPosition() {
			Vector3 position;
			Vector3 forward;

			if (!engine::GetCameraTransform(position, forward)) {
				return;
			}

			std::lock_guard guard(g_positionLock);
			g_listenerPosition = position;
		}

		// Thread: Main
		std::uint32_t g_lastLoggedEnvironment = 0;
		void* g_lastLoggedSpace = nullptr;

	}

	// Evaluates the player's current environment and publishes it with the cell kind and the camera position.
	// Uses the engine's current acoustic space or the cell's space.
	// If none is defined, use fallbacks.
	//
	// Thread: Main (each frame)
	void UpdateListenerEnvironment() {
		void* player = engine::GetPlayer();
		void* cell = engine::GetParentCell(player);

		if (!cell) {
			return;
		}

		const bool isInterior = Field<std::uint8_t>(cell, engine::kCell_Flags) & 1;
		g_listenerInExterior.store(!isInterior, std::memory_order_relaxed);
		UpdateListenerPosition();

		void* space = mem::Global<void*>(engine::kCurrentAcousticSpace);
		const char* source = "current ASPC";

		if (!space) {
			space = engine::GetCellAcousticSpace(cell);
			source = "cell ASPC";
		}

		std::uint32_t environment = 0;

		if (space) {
			environment = Field<std::uint32_t>(space, engine::kAspc_EnvironmentType);
		}

		// If no space or space with type `None`, use fallback for cell kind.
		if (environment == 0 || environment > kEnvironmentCount) {
			const config::ReverbSettings& settings = config::Get().reverb;

			if (isInterior) {
				environment = settings.interiorFallback;
				source = "interior fallback";
			} else {
				environment = settings.exteriorFallback;
				source = "exterior fallback";
			}
		}

		g_listenerEnvironment.store(environment, std::memory_order_relaxed);

		if (environment == g_lastLoggedEnvironment && space == g_lastLoggedSpace) {
			return;
		}

		g_lastLoggedEnvironment = environment;
		g_lastLoggedSpace = space;

		if (space) {
			SEA_LOG("[Reverb] Listener Environment: %s (%s %s)", engine::EnvironmentTypeName(environment), source,
				engine::GetEditorID(space));
		} else {
			SEA_LOG("[Reverb] Listener Environment: %s (%s)", engine::EnvironmentTypeName(environment), source);
		}
	}

	// Returns the environment (ANAM) from the last update, 0 = unknown.
	//
	// Thread: Any
	std::uint32_t GetListenerEnvironment() {
		return g_listenerEnvironment.load(std::memory_order_relaxed);
	}

	// Checks if the player was in an exterior cell at the last update.
	//
	// Thread: Any
	bool IsListenerInExterior() {
		return g_listenerInExterior.load(std::memory_order_relaxed);
	}

	// Returns the camera position from the last update. Game units.
	//
	// Thread: Any
	Vector3 GetListenerPosition() {
		std::lock_guard guard(g_positionLock);

		return g_listenerPosition;
	}

}
