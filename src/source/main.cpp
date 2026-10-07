// xNVSE plugin entry points for Saint's Experimental Acoustics.

#include "nvse_minimal.h"

#include "game.h"
#include "log.h"
#include "probe_cell.h"
#include "probe_sound.h"
#include "reverb.h"

#include <Windows.h>

#include <cstdlib>
#include <string>

namespace {
	constexpr const char* kPluginName = "SaintsExperimentalAcoustics";
	constexpr std::uint32_t kPluginVersion = 2;

	PluginHandle g_pluginHandle = kPluginHandle_Invalid;
	std::string g_runtimeDirectory;

	int g_bypassKey = 0;  // Virtual-key code, 0 = no key
	bool g_bypassKeyWasDown = false;

	std::string IniPath() {
		return g_runtimeDirectory + "Data\\NVSE\\Plugins\\SaintsAcoustics.ini";
	}

	bool ReadIniBool(const std::string& iniPath, const char* section, const char* key, bool fallback) {
		const UINT value = GetPrivateProfileIntA(section, key, static_cast<INT>(fallback), iniPath.c_str());

		return value != 0;
	}

	float ReadIniFloat(const std::string& iniPath, const char* section, const char* key, float fallback) {
		char text[32];
		GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath.c_str());

		char* parseEnd = nullptr;
		const float value = std::strtof(text, &parseEnd);

		if (parseEnd == text) {
			return fallback;
		}

		return value;
	}

	// Reads decimal or 0x hex values. 
	// GetPrivateProfileInt reads only decimal values.
	int ReadIniInt(const std::string& iniPath, const char* section, const char* key, int fallback) {
		char text[32];
		GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath.c_str());

		char* parseEnd = nullptr;
		const long value = std::strtol(text, &parseEnd, 0);

		if (parseEnd == text) {
			return fallback;
		}

		return static_cast<int>(value);
	}

	std::uint32_t ReadIniEnvironment(const std::string& iniPath, const char* section, const char* key, std::uint32_t fallback) {
		char text[32];
		GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath.c_str());

		if (text[0] == '\0') {
			return fallback;
		}

		return sea::game::EnvironmentTypeFromName(text, fallback);
	}

	void LoadReverbConfig() {
		const std::string iniPath = IniPath();
		sea::reverb::Config config;

		config.enabled = ReadIniBool(iniPath, "Reverb", "bEnabled", config.enabled);
		config.wetLevelDb = ReadIniFloat(iniPath, "Reverb", "fWetLevel", config.wetLevelDb);
		config.roomBoostDb = ReadIniFloat(iniPath, "Reverb", "fRoomBoost", config.roomBoostDb);
		config.interiorFallback = ReadIniEnvironment(iniPath, "Reverb", "sInteriorFallback", config.interiorFallback);
		config.exteriorFallback = ReadIniEnvironment(iniPath, "Reverb", "sExteriorFallback", config.exteriorFallback);
		g_bypassKey = ReadIniInt(iniPath, "Reverb", "iBypassKey", VK_END);

		config.sendVoice3D = ReadIniFloat(iniPath, "Sends", "fVoice3D", config.sendVoice3D);
		config.sendVoice2D = ReadIniFloat(iniPath, "Sends", "fVoice2D", config.sendVoice2D);
		config.sendWeapons = ReadIniFloat(iniPath, "Sends", "fWeapons", config.sendWeapons);
		config.sendFootsteps = ReadIniFloat(iniPath, "Sends", "fFootsteps", config.sendFootsteps);
		config.sendLoops3D = ReadIniFloat(iniPath, "Sends", "fLoops3D", config.sendLoops3D);
		config.sendLoops2D = ReadIniFloat(iniPath, "Sends", "fLoops2D", config.sendLoops2D);
		config.sendRegion = ReadIniFloat(iniPath, "Sends", "fRegion", config.sendRegion);
		config.sendDefault3D = ReadIniFloat(iniPath, "Sends", "fDefault3D", config.sendDefault3D);
		config.sendDefault2D = ReadIniFloat(iniPath, "Sends", "fDefault2D", config.sendDefault2D);

		config.forceEnvironment = ReadIniEnvironment(iniPath, "Debug", "sForceEnvironment", 0);
		config.readbackCount = static_cast<std::uint32_t>(ReadIniInt(iniPath, "Debug", "iReadbackCount", 0));

		sea::reverb::Configure(config);

		const char* forceEnvironmentName = "off";

		if (config.forceEnvironment) {
			forceEnvironmentName = sea::game::EnvironmentTypeName(config.forceEnvironment);
		}

		SEA_LOG("Config reverb: Enabled=%d Wet=%.1fdB RoomBoost=%.1fdB InteriorFallback=%s ExteriorFallback=%s BypassKey=0x%X "
				"Force=%s Readback=%u",
			config.enabled, config.wetLevelDb, config.roomBoostDb, sea::game::EnvironmentTypeName(config.interiorFallback),
			sea::game::EnvironmentTypeName(config.exteriorFallback), g_bypassKey, forceEnvironmentName, config.readbackCount);
		
		SEA_LOG("Config sends (dB): Voice3D=%.1f Voice2D=%.1f Weapons=%.1f Footsteps=%.1f Loops3D=%.1f Loops2D=%.1f "
				"Region=%.1f Default3D=%.1f Default2D=%.1f",
			config.sendVoice3D, config.sendVoice2D, config.sendWeapons, config.sendFootsteps, config.sendLoops3D,
			config.sendLoops2D, config.sendRegion, config.sendDefault3D, config.sendDefault2D);
	}

	bool GameHasFocus() {
		DWORD foregroundProcessId = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);

		return foregroundProcessId == GetCurrentProcessId();
	}

	void PollBypassKey() {
		if (!g_bypassKey || !sea::reverb::GetConfig().enabled) {
			return;
		}

		const bool keyDown = (GetAsyncKeyState(g_bypassKey) & 0x8000) != 0 && GameHasFocus();

		if (keyDown && !g_bypassKeyWasDown) {
			const bool reverbActive = sea::reverb::ToggleBypass();

			if (reverbActive) {
				sea::game::ShowNotification("Acoustics Processing: ON");
			} else {
				sea::game::ShowNotification("Acoustics Processing: OFF");
			}
		}

		g_bypassKeyWasDown = keyDown;
	}

	sea::probe::SoundProbeConfig LoadSoundConfig() {
		const std::string iniPath = IniPath();
		sea::probe::SoundProbeConfig config;

		config.logPlay = ReadIniBool(iniPath, "Debug", "bLogSoundPlay", config.logPlay);
		config.logEnvironmentChange = ReadIniBool(iniPath, "Debug", "bLogSoundEnvironment", config.logEnvironmentChange);
		config.layoutProbeCount = GetPrivateProfileIntA("Debug", "iLayoutProbeCount", config.layoutProbeCount, iniPath.c_str());

		SEA_LOG("Config probes: LogSoundPlay=%d LogSoundEnvironment=%d LayoutProbeCount=%u (%s)", config.logPlay,
			config.logEnvironmentChange, config.layoutProbeCount, iniPath.c_str());

		return config;
	}

	void OnMessage(NVSEMessagingInterface::Message* message) {
		switch (message->type) {
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			sea::probe::PollPlayerAcoustics();
			sea::reverb::UpdateListenerEnvironment();
			PollBypassKey();
			break;

		case NVSEMessagingInterface::kMessage_PostLoadGame:
			SEA_LOG("Game loaded.");
			sea::probe::ResetPlayerAcoustics();
			break;

		case NVSEMessagingInterface::kMessage_ExitToMainMenu:
			SEA_LOG("Exited to main menu.");
			sea::probe::ResetPlayerAcoustics();
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
	g_pluginHandle = nvse->GetPluginHandle();
	g_runtimeDirectory = nvse->GetRuntimeDirectory();

	sea::log::Open((g_runtimeDirectory + "SaintsAcoustics.log").c_str());
	SEA_LOG("Loading %s v%u (xNVSE %08X, runtime %08X).", kPluginName, kPluginVersion, nvse->nvseVersion, nvse->runtimeVersion);

	auto* messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));

	if (!messaging || messaging->version < 4 || !messaging->RegisterListener(g_pluginHandle, "NVSE", OnMessage)) {
		SEA_LOG("Error: Messaging interface is unavailable or too old.");

		return false;
	}

	LoadReverbConfig();

	if (!sea::probe::InstallSoundHooks(LoadSoundConfig())) {
		SEA_LOG("Error: Could not install sound hooks.");
	}

	SEA_LOG("Plugin loaded.");

	return true;
}

}
