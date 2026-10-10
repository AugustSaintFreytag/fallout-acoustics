// xNVSE plugin entry points for Saint's Experimental Acoustics.

#include "nvse/nvse_minimal.h"

#include "config/settings.h"
#include "debug/cell_probe.h"
#include "debug/grid_probe.h"
#include "debug/pick_census.h"
#include "debug/sound_probe.h"
#include "engine/holotapes.h"
#include "fixes/holotape_duration.h"
#include "fixes/load_holotape_sounds.h"
#include "fixes/open_close_sounds.h"
#include "hooks/havok_hooks.h"
#include "hooks/request_hooks.h"
#include "hooks/sound_hooks.h"
#include "hooks/voice_hooks.h"
#include "input/hotkeys.h"
#include "occlusion/occlusion.h"
#include "reverb/listener.h"
#include "utils/log.h"
#include "utils/threads.h"

#include <cstdint>
#include <string>

namespace {

	constexpr const char* kPluginName = "SaintsExperimentalAcoustics";
	constexpr std::uint32_t kPluginVersion = 2;

	// Handles NVSE messages. Runs per-frame work on each main loop message.
	// Resets the acoustics probe on load and on exit to the menu.
	void OnMessage(NVSEMessagingInterface::Message* message) {
		switch (message->type) {
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			sea::threads::RememberMainThread();
			sea::debug::PollAndLogPlayerAcoustics();
			sea::debug::PollGridProbe();
			sea::debug::PollPickCensus();
			sea::reverb::UpdateListenerEnvironment();
			sea::engine::UpdateHolotapeSounds();
			sea::fixes::UpdateHolotapeDuration();
			sea::occlusion::UpdateOcclusion();
			sea::input::PollHotkeys();
			break;

		case NVSEMessagingInterface::kMessage_PreLoadGame:
			sea::fixes::OnPreLoadGame();
			break;

		case NVSEMessagingInterface::kMessage_PostLoadGame:
			SEA_LOG("Game loaded.");
			sea::debug::ResetPlayerAcoustics();
			sea::fixes::OnPostLoadGame();
			break;

		case NVSEMessagingInterface::kMessage_ExitToMainMenu:
			SEA_LOG("Exited to main menu.");
			sea::debug::ResetPlayerAcoustics();
			break;

		default:
			break;
		}
	}

}

extern "C" {

// Accepts only the game (not the editor) at runtime 1.4.0.525.
__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;

	if (nvse->isEditor) {
		return false;
	}

	return nvse->runtimeVersion == RUNTIME_VERSION_1_4_0_525;
}

// Opens the log, registers for messages, reads the INI and installs the hooks.
// A hook that fails is logged. The rest of the plugin still loads.
__declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface* nvse) {
	sea::threads::RememberMainThread();

	const PluginHandle pluginHandle = nvse->GetPluginHandle();
	const std::string runtimeDirectory = nvse->GetRuntimeDirectory();

	sea::log::Open((runtimeDirectory + "SaintsAcoustics.log").c_str());
	SEA_LOG("Loading %s v%u (xNVSE %08X, runtime %08X).", kPluginName, kPluginVersion, nvse->nvseVersion, nvse->runtimeVersion);

	auto* messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));

	if (!messaging || messaging->version < 4 || !messaging->RegisterListener(pluginHandle, "NVSE", OnMessage)) {
		SEA_LOG("Error: Messaging interface is unavailable or defunct.");

		return false;
	}

	sea::config::Load(runtimeDirectory + "Data\\NVSE\\Plugins\\SaintsAcoustics.ini");
	sea::debug::InitializeSoundProbe();

	if (!sea::hooks::InstallSoundHooks()) {
		SEA_LOG("Error: Could not install sound hooks.");
	}

	if (!sea::hooks::InstallVoiceHooks()) {
		SEA_LOG("Error: Could not install voice modulation hooks.");
	}

	if (!sea::fixes::InstallOpenCloseSoundFix()) {
		SEA_LOG("Error: Could not install the open/close sound fix.");
	}

	if (!sea::hooks::InstallRequestHooks()) {
		SEA_LOG("Error: Could not install all sound request hooks.");
	}

	if (!sea::hooks::InstallHavokHooks()) {
		SEA_LOG("Error: Could not install the Havok pick hooks.");
	}

	SEA_LOG("Plugin loaded.");

	return true;
}

}
