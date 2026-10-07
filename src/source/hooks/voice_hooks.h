#pragma once

namespace sea::hooks {
	// Replaces the vanilla voice modulation check in Actor::VoiceSoundFunction with the cover classification.
	// Call one time, from `NVSEPlugin_Load`, after `config::Load`. Does nothing if `[VoiceFilters] bEnabled=0`.
	bool InstallVoiceHooks();
}
