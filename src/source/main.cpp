// xNVSE plugin entry points for Saint's Experimental Acoustics.

#include "nvse/nvse_minimal.h"

#include "config/settings.h"
#include "debug/cell_probe.h"
#include "debug/sound_probe.h"
#include "hooks/sound_hooks.h"
#include "input/hotkeys.h"
#include "reverb/listener.h"
#include "utils/log.h"

#include <cstdint>
#include <string>

namespace {
	constexpr const char* kPluginName = "SaintsExperimentalAcoustics";
	constexpr std::uint32_t kPluginVersion = 2;

	void OnMessage(NVSEMessagingInterface::Message* message) {
		switch (message->type) {
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			sea::debug::PollPlayerAcoustics();
			sea::reverb::UpdateListenerEnvironment();
			sea::input::PollHotkeys();
			break;

		case NVSEMessagingInterface::kMessage_PostLoadGame:
			SEA_LOG("Game loaded.");
			sea::debug::ResetPlayerAcoustics();
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

__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;

	if (nvse->isEditor) {
		return false;
	}

	return nvse->runtimeVersion == RUNTIME_VERSION_1_4_0_525;
}

__declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface* nvse) {
	const PluginHandle pluginHandle = nvse->GetPluginHandle();
	const std::string runtimeDirectory = nvse->GetRuntimeDirectory();

	sea::log::Open((runtimeDirectory + "SaintsAcoustics.log").c_str());
	SEA_LOG("Loading %s v%u (xNVSE %08X, runtime %08X).", kPluginName, kPluginVersion, nvse->nvseVersion, nvse->runtimeVersion);

	auto* messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));

	if (!messaging || messaging->version < 4 || !messaging->RegisterListener(pluginHandle, "NVSE", OnMessage)) {
		SEA_LOG("Error: Messaging interface is unavailable or too old.");

		return false;
	}

	sea::config::Load(runtimeDirectory + "Data\\NVSE\\Plugins\\SaintsAcoustics.ini");
	sea::debug::InitializeSoundProbe();

	if (!sea::hooks::InstallSoundHooks()) {
		SEA_LOG("Error: Could not install sound hooks.");
	}

	SEA_LOG("Plugin loaded.");

	return true;
}

}
