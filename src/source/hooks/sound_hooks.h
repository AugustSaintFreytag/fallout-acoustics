#pragma once

namespace sea::hooks {
	// Patches the BSWin32GameSound vtable, should be called one time, from `NVSEPlugin_Load`.
	// Call after `config::Load`.
	bool InstallSoundHooks();
}
