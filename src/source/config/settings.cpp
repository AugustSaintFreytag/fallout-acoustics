#include "config/settings.h"

#include "engine/environment.h"
#include "utils/log.h"

#include <Windows.h>

#include <cstdlib>

namespace sea::config {
	namespace {
		Settings g_settings;

		bool ReadBool(const char* iniPath, const char* section, const char* key, bool fallback) {
			const UINT value = GetPrivateProfileIntA(section, key, static_cast<INT>(fallback), iniPath);

			return value != 0;
		}

		float ReadFloat(const char* iniPath, const char* section, const char* key, float fallback) {
			char text[32];
			GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath);

			char* parseEnd = nullptr;
			const float value = std::strtof(text, &parseEnd);

			if (parseEnd == text) {
				return fallback;
			}

			return value;
		}

		// Reads decimal or 0x hex values.
		// GetPrivateProfileInt reads only decimal values.
		int ReadInt(const char* iniPath, const char* section, const char* key, int fallback) {
			char text[32];
			GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath);

			char* parseEnd = nullptr;
			const long value = std::strtol(text, &parseEnd, 0);

			if (parseEnd == text) {
				return fallback;
			}

			return static_cast<int>(value);
		}

		std::uint32_t ReadEnvironment(const char* iniPath, const char* section, const char* key, std::uint32_t fallback) {
			char text[32];
			GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath);

			if (text[0] == '\0') {
				return fallback;
			}

			return engine::EnvironmentTypeFromName(text, fallback);
		}

		void LoadReverb(const char* iniPath) {
			ReverbSettings& reverb = g_settings.reverb;

			reverb.enabled = ReadBool(iniPath, "Reverb", "bEnabled", reverb.enabled);
			reverb.wetLevelDb = ReadFloat(iniPath, "Reverb", "fWetLevel", reverb.wetLevelDb);
			reverb.roomBoostDb = ReadFloat(iniPath, "Reverb", "fRoomBoost", reverb.roomBoostDb);
			reverb.interiorFallback = ReadEnvironment(iniPath, "Reverb", "sInteriorFallback", reverb.interiorFallback);
			reverb.exteriorFallback = ReadEnvironment(iniPath, "Reverb", "sExteriorFallback", reverb.exteriorFallback);

			HotkeySettings& hotkeys = g_settings.hotkeys;
			hotkeys.bypassKey = ReadInt(iniPath, "Reverb", "iBypassKey", hotkeys.bypassKey);
		}

		void LoadSends(const char* iniPath) {
			SendSettings& sends = g_settings.sends;

			sends.voice3D = ReadFloat(iniPath, "Sends", "fVoice3D", sends.voice3D);
			sends.voice2D = ReadFloat(iniPath, "Sends", "fVoice2D", sends.voice2D);
			sends.weapons = ReadFloat(iniPath, "Sends", "fWeapons", sends.weapons);
			sends.footsteps = ReadFloat(iniPath, "Sends", "fFootsteps", sends.footsteps);
			sends.loops3D = ReadFloat(iniPath, "Sends", "fLoops3D", sends.loops3D);
			sends.loops2D = ReadFloat(iniPath, "Sends", "fLoops2D", sends.loops2D);
			sends.region = ReadFloat(iniPath, "Sends", "fRegion", sends.region);
			sends.default3D = ReadFloat(iniPath, "Sends", "fDefault3D", sends.default3D);
			sends.default2D = ReadFloat(iniPath, "Sends", "fDefault2D", sends.default2D);
		}

		void LoadDebug(const char* iniPath) {
			DebugSettings& debug = g_settings.debug;

			debug.forceEnvironment = ReadEnvironment(iniPath, "Debug", "sForceEnvironment", debug.forceEnvironment);
			debug.readbackCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iReadbackCount", debug.readbackCount));
			debug.logSoundPlay = ReadBool(iniPath, "Debug", "bLogSoundPlay", debug.logSoundPlay);
			debug.logSoundEnvironment = ReadBool(iniPath, "Debug", "bLogSoundEnvironment", debug.logSoundEnvironment);
			debug.layoutProbeCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iLayoutProbeCount", debug.layoutProbeCount));
			debug.deferEaxSets = ReadBool(iniPath, "Debug", "bDeferEaxSets", debug.deferEaxSets);
			debug.logRouteTiming = ReadBool(iniPath, "Debug", "bLogRouteTiming", debug.logRouteTiming);
		}

		void LogSettings() {
			const ReverbSettings& reverb = g_settings.reverb;
			const SendSettings& sends = g_settings.sends;
			const DebugSettings& debug = g_settings.debug;

			SEA_LOG("Config reverb: Enabled=%d Wet=%.1fdB RoomBoost=%.1fdB InteriorFallback=%s ExteriorFallback=%s BypassKey=0x%X",
				reverb.enabled, reverb.wetLevelDb, reverb.roomBoostDb, engine::EnvironmentTypeName(reverb.interiorFallback),
				engine::EnvironmentTypeName(reverb.exteriorFallback), g_settings.hotkeys.bypassKey);

			SEA_LOG("Config sends (dB): Voice3D=%.1f Voice2D=%.1f Weapons=%.1f Footsteps=%.1f Loops3D=%.1f Loops2D=%.1f "
					"Region=%.1f Default3D=%.1f Default2D=%.1f",
				sends.voice3D, sends.voice2D, sends.weapons, sends.footsteps, sends.loops3D, sends.loops2D, sends.region,
				sends.default3D, sends.default2D);

			const char* forceEnvironmentName = "off";

			if (debug.forceEnvironment) {
				forceEnvironmentName = engine::EnvironmentTypeName(debug.forceEnvironment);
			}

			SEA_LOG("Config debug: Force=%s Readback=%u LogSoundPlay=%d LogSoundEnvironment=%d LayoutProbeCount=%u "
					"DeferEaxSets=%d LogRouteTiming=%d",
				forceEnvironmentName, debug.readbackCount, debug.logSoundPlay, debug.logSoundEnvironment,
				debug.layoutProbeCount, debug.deferEaxSets, debug.logRouteTiming);
		}
	}

	void Load(const std::string& iniPath) {
		LoadReverb(iniPath.c_str());
		LoadSends(iniPath.c_str());
		LoadDebug(iniPath.c_str());

		SEA_LOG("Config loaded from %s.", iniPath.c_str());
		LogSettings();
	}

	const Settings& Get() {
		return g_settings;
	}
}
