#include "occlusion/registry.h"

#include "utils/timing.h"

#include <mutex>
#include <unordered_map>

namespace sea::occlusion {

	namespace {

		constexpr double kExpiryMs = 2000.0;

		struct Entry {
			Vector3 position;
			std::int64_t lastPublished = 0;
			std::int32_t targetMb = 0;
			bool hasTarget = false;
			std::string path;
		};

		// Live 3D sounds shared between audio thread and main thread.
		// Audio thread publishes positions and reads targets. Main thread reads positions and sets targets.
		// Main thread never touches sound objects. Audio thread never touches Havok.
		//
		// Thread: Any (under `g_lock`)
		std::mutex g_lock;
		std::unordered_map<std::uint32_t, Entry> g_sounds;

	}

	// Adds or updates a sound with its position. Path is copied only when the sound is new.
	//
	// Thread: Audio
	void PublishSound(std::uint32_t soundId, const Vector3& position, const char* path) {
		const std::int64_t now = timing::Now();

		std::lock_guard guard(g_lock);

		const auto [entry, inserted] = g_sounds.try_emplace(soundId);

		if (inserted && path) {
			entry->second.path = path;
		}

		entry->second.position = position;
		entry->second.lastPublished = now;
	}

	// Reads the occlusion target of a sound. Returns false if the main thread has no target for it yet.
	//
	// Thread: Audio
	bool GetTarget(std::uint32_t soundId, std::int32_t& occlusionMb) {
		std::lock_guard guard(g_lock);

		const auto entry = g_sounds.find(soundId);

		if (entry == g_sounds.end() || !entry->second.hasTarget) {
			return false;
		}

		occlusionMb = entry->second.targetMb;

		return true;
	}

	// Returns all live sounds. Removes sounds that were not published for 2 seconds.
	//
	// Thread: Main
	std::vector<LiveSound> CollectSounds() {
		const std::int64_t now = timing::Now();
		std::vector<LiveSound> sounds;

		std::lock_guard guard(g_lock);

		sounds.reserve(g_sounds.size());

		for (auto entry = g_sounds.begin(); entry != g_sounds.end();) {
			const double ageMs = timing::TicksToMilliseconds(static_cast<double>(now - entry->second.lastPublished));

			if (ageMs > kExpiryMs) {
				entry = g_sounds.erase(entry);
				continue;
			}

			sounds.push_back({entry->first, entry->second.position});
			++entry;
		}

		return sounds;
	}

	// Sets the occlusion target of a sound in mB (≤ 0). Ignores unknown sounds.
	//
	// Thread: Main
	void SetTarget(std::uint32_t soundId, std::int32_t occlusionMb) {
		std::lock_guard guard(g_lock);

		const auto entry = g_sounds.find(soundId);

		if (entry == g_sounds.end()) {
			return;
		}

		entry->second.targetMb = occlusionMb;
		entry->second.hasTarget = true;
	}

	// Returns the file path of a sound for the log. Returns "" for an unknown sound.
	//
	// Thread: Main
	std::string GetPath(std::uint32_t soundId) {
		std::lock_guard guard(g_lock);

		const auto entry = g_sounds.find(soundId);

		if (entry == g_sounds.end()) {
			return {};
		}

		return entry->second.path;
	}

}
