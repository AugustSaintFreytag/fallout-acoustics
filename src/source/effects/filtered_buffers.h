#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace sea::effects {

	// A record of filtered sound buffers with the content hash after filtering.
	//
	// The engine reuses buffers. The same hash at the next `Play` means that the engine did not write new data,
	// so the buffer is already filtered.
	struct FilteredBuffers {
		static constexpr std::size_t kMaxTrackedBuffers = 512;

		std::mutex lock;
		std::unordered_map<std::uintptr_t, std::uint64_t> contentHashes;

		bool Contains(std::uintptr_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(lock);
			const auto entry = contentHashes.find(buffer);

			if (entry == contentHashes.end()) {
				return false;
			}

			return entry->second == contentHash;
		}

		void Remember(std::uintptr_t buffer, std::uint64_t contentHash) {
			std::lock_guard guard(lock);

			// Released buffers are never removed, so the map is cleared when it grows too large.
			if (contentHashes.size() >= kMaxTrackedBuffers) {
				contentHashes.clear();
			}

			contentHashes[buffer] = contentHash;
		}
	};

}
