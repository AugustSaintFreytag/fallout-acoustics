#include "engine/ui.h"

#include "engine/addresses.h"

#include <cstdint>

namespace sea::engine {
	void ShowNotification(const char* message) {
		using QueueUIMessageFn = bool(__cdecl*)(const char* message, std::uint32_t emotion, const char* ddsPath,
			const char* soundName, float seconds, bool maybeNextToDisplay);

		const auto queueUIMessage = reinterpret_cast<QueueUIMessageFn>(kQueueUIMessage);
		queueUIMessage(message, 0, nullptr, nullptr, 2.0f, false);
	}
}
