#pragma once

namespace sea::engine {

	// A kind of weapon fire sound, as told by its file path.
	enum class GunfireKind {
		None,
		Shot,  // Single shot
		Loop,  // Automatic fire, several shots in one file
	};

	GunfireKind ClassifyGunfirePath(const char* path);

	bool IsAmbiencePath(const char* path);

	bool IsExplosionPath(const char* path);

	bool IsHolotapeStartStopPath(const char* path);

}
