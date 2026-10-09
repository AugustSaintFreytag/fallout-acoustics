#include "engine/gunfire_path.h"

#include <cctype>
#include <cstddef>
#include <string_view>

namespace sea::engine {

	namespace {

		constexpr std::size_t kMaxPathLength = 260;

		constexpr std::string_view kWeaponFolder = "sound\\fx\\wpn\\";
		constexpr std::string_view kFireMarker = "_fire";
		constexpr std::string_view kLoopMarker = "loop";
		constexpr std::string_view kLoopSuffix = "_lp";

		// Copies the given path in lower case with backslashes into the supplied buffer.
		// Returns the length of the copy. Longer paths are cut at the buffer size.
		std::size_t NormalizePath(const char* path, char* buffer) {
			std::size_t length = 0;

			while (length < kMaxPathLength && path[length] != '\0') {
				char character = static_cast<char>(std::tolower(static_cast<unsigned char>(path[length])));

				if (character == '/') {
					character = '\\';
				}

				buffer[length] = character;
				++length;
			}

			return length;
		}

		// Returns the file name of the given path without its extension.
		std::string_view FileStem(std::string_view path) {
			const std::size_t slash = path.rfind('\\');

			if (slash != std::string_view::npos) {
				path.remove_prefix(slash + 1);
			}

			const std::size_t dot = path.rfind('.');

			if (dot != std::string_view::npos) {
				path.remove_suffix(path.size() - dot);
			}

			return path;
		}

		// Checks if the given file name has the fire marker as a whole word.
		// The marker is followed by the end, `_` or a digit. `_fire_2d` and `_fire01` count, `_firelance` does not.
		bool HasFireMarker(std::string_view stem) {
			std::size_t position = stem.find(kFireMarker);

			while (position != std::string_view::npos) {
				const std::size_t next = position + kFireMarker.size();

				if (next == stem.size() || stem[next] == '_' || std::isdigit(static_cast<unsigned char>(stem[next]))) {
					return true;
				}

				position = stem.find(kFireMarker, next);
			}

			return false;
		}

	}

	// Returns the kind of weapon fire the given sound file path names.
	// Vanilla and most mods keep weapon sounds in `sound\fx\wpn\` and fire sounds have `_fire` as a word in the file name.
	// Names with `loop` or ending in `_lp` are automatic fire.
	GunfireKind ClassifyGunfirePath(const char* path) {
		char buffer[kMaxPathLength];
		const std::string_view normalized(buffer, NormalizePath(path, buffer));

		if (normalized.find(kWeaponFolder) == std::string_view::npos) {
			return GunfireKind::None;
		}

		const std::string_view stem = FileStem(normalized);

		if (!HasFireMarker(stem)) {
			return GunfireKind::None;
		}

		if (stem.find(kLoopMarker) != std::string_view::npos || stem.ends_with(kLoopSuffix)) {
			return GunfireKind::Loop;
		}

		return GunfireKind::Shot;
	}

}
