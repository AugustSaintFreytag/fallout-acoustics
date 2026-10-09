#include "config/settings.h"

#include "engine/environment.h"
#include "utils/log.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
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

		// Reads an integer, decimal or `0x` hex. Returns `fallback` if the key is missing or not a number.
		//
		// `GetPrivateProfileInt` reads only decimal values.
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

		// Reads a list of collision layers separated by commas or spaces, such as "37, 1, 6".
		// Returns `fallback` if the list has no layer in [1, 127].
		std::vector<std::uint8_t> ReadLayerList(const char* iniPath, const char* section, const char* key,
			const std::vector<std::uint8_t>& fallback) {
			char text[128];
			GetPrivateProfileStringA(section, key, "", text, sizeof(text), iniPath);

			std::vector<std::uint8_t> layers;
			const char* position = text;

			while (*position != '\0') {
				char* parseEnd = nullptr;
				const long value = std::strtol(position, &parseEnd, 0);

				if (parseEnd == position) {
					++position;
					continue;
				}

				if (value > 0 && value < 128) {
					layers.push_back(static_cast<std::uint8_t>(value));
				}

				position = parseEnd;
			}

			if (layers.empty()) {
				return fallback;
			}

			return layers;
		}

		// Reads an environment by name, such as "MediumRoom". 
		// Returns `fallback` if the key is missing or unknown.
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
			reverb.wetLevel = ReadFloat(iniPath, "Reverb", "fWetLevel", reverb.wetLevel);
			reverb.roomBoost = ReadFloat(iniPath, "Reverb", "fRoomBoost", reverb.roomBoost);
			reverb.radioBoost = ReadFloat(iniPath, "Reverb", "fRadioBoost", reverb.radioBoost);
			reverb.interiorFallback = ReadEnvironment(iniPath, "Reverb", "sInteriorFallback", reverb.interiorFallback);
			reverb.exteriorFallback = ReadEnvironment(iniPath, "Reverb", "sExteriorFallback", reverb.exteriorFallback);

			// The reverb bypass key is in `[Reverb]`, but is stored with the other hotkeys.
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
			sends.radio3D = ReadFloat(iniPath, "Sends", "fRadio3D", sends.radio3D);
			sends.default3D = ReadFloat(iniPath, "Sends", "fDefault3D", sends.default3D);
			sends.default2D = ReadFloat(iniPath, "Sends", "fDefault2D", sends.default2D);
		}

		void LoadVoiceFilters(const char* iniPath) {
			VoiceFilterSettings& voiceFilters = g_settings.voiceFilters;

			voiceFilters.enabled = ReadBool(iniPath, "VoiceFilters", "bEnabled", voiceFilters.enabled);
		}

		void LoadOcclusion(const char* iniPath) {
			OcclusionSettings& occlusion = g_settings.occlusion;

			occlusion.enabled = ReadBool(iniPath, "Occlusion", "bEnabled", occlusion.enabled);
			occlusion.wallLevel = ReadFloat(iniPath, "Occlusion", "fWallLevel", occlusion.wallLevel);
			occlusion.maxOcclusion = ReadFloat(iniPath, "Occlusion", "fMaxOcclusion", occlusion.maxOcclusion);
			occlusion.lfRatio = ReadFloat(iniPath, "Occlusion", "fLFRatio", occlusion.lfRatio);
			occlusion.roomRatio = ReadFloat(iniPath, "Occlusion", "fRoomRatio", occlusion.roomRatio);
			occlusion.maxDistance = ReadFloat(iniPath, "Occlusion", "fMaxDistance", occlusion.maxDistance);
			occlusion.rayBudget = ReadInt(iniPath, "Occlusion", "iRayBudget", occlusion.rayBudget);
			occlusion.refreshInterval = ReadFloat(iniPath, "Occlusion", "fRefreshInterval", occlusion.refreshInterval);
			occlusion.attackTime = ReadFloat(iniPath, "Occlusion", "fAttackTime", occlusion.attackTime);
			occlusion.releaseTime = ReadFloat(iniPath, "Occlusion", "fReleaseTime", occlusion.releaseTime);
			occlusion.bypassKey = ReadInt(iniPath, "Occlusion", "iBypassKey", occlusion.bypassKey);
		}

		void LoadDistance(const char* iniPath) {
			DistanceSettings& distance = g_settings.distance;

			const float factor = ReadFloat(iniPath, "Distance", "fDistanceAttenuationFactor", distance.attenuationFactor);
			distance.attenuationFactor = std::clamp(factor, 0.1f, 10.0f);
		}

		void LoadFixes(const char* iniPath) {
			FixSettings& fixes = g_settings.fixes;

			fixes.openCloseSounds = ReadBool(iniPath, "Fixes", "bFixDoubleOpenCloseSounds", fixes.openCloseSounds);
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
			debug.logVoiceCover = ReadBool(iniPath, "Debug", "bLogVoiceCover", debug.logVoiceCover);

			debug.rayProbeKey = ReadInt(iniPath, "Debug", "iRayProbeKey", debug.rayProbeKey);
			debug.rayProbeLayers = ReadLayerList(iniPath, "Debug", "sRayProbeLayers", debug.rayProbeLayers);
			debug.rayProbeRange = ReadFloat(iniPath, "Debug", "fRayProbeRange", debug.rayProbeRange);
			debug.probeSoundRequests = ReadBool(iniPath, "Debug", "bProbeSoundRequests", debug.probeSoundRequests);
			debug.probeSoundUpdate = ReadBool(iniPath, "Debug", "bProbeSoundUpdate", debug.probeSoundUpdate);
			debug.testOcclusion = ReadInt(iniPath, "Debug", "iTestOcclusion", debug.testOcclusion);
			debug.occlusionTestKey = ReadInt(iniPath, "Debug", "iOcclusionTestKey", debug.occlusionTestKey);
			debug.probeGrid = ReadBool(iniPath, "Debug", "bProbeGrid", debug.probeGrid);
			debug.probePickThreads = ReadBool(iniPath, "Debug", "bProbePickThreads", debug.probePickThreads);
			debug.logOcclusion = ReadBool(iniPath, "Debug", "bLogOcclusion", debug.logOcclusion);
		}

		void LogSettings() {
			const ReverbSettings& reverb = g_settings.reverb;
			const SendSettings& sends = g_settings.sends;
			const DebugSettings& debug = g_settings.debug;

			SEA_LOG("Config reverb: Enabled=%d Wet=%.1fdB RoomBoost=%.1fdB RadioBoost=%.1fdB InteriorFallback=%s "
					"ExteriorFallback=%s BypassKey=0x%X",
				reverb.enabled, reverb.wetLevel, reverb.roomBoost, reverb.radioBoost, engine::EnvironmentTypeName(reverb.interiorFallback),
				engine::EnvironmentTypeName(reverb.exteriorFallback), g_settings.hotkeys.bypassKey);

			SEA_LOG("Config sends (dB): Voice3D=%.1f Voice2D=%.1f Weapons=%.1f Footsteps=%.1f Loops3D=%.1f Loops2D=%.1f "
					"Region=%.1f Radio3D=%.1f Default3D=%.1f Default2D=%.1f",
				sends.voice3D, sends.voice2D, sends.weapons, sends.footsteps, sends.loops3D, sends.loops2D, sends.region,
				sends.radio3D, sends.default3D, sends.default2D);

			SEA_LOG("Config voice filters: Enabled=%d", g_settings.voiceFilters.enabled);

			SEA_LOG("Config distance: AttenuationFactor=%.2f", g_settings.distance.attenuationFactor);
			SEA_LOG("Config fixes: OpenCloseSounds=%d", g_settings.fixes.openCloseSounds);

			const OcclusionSettings& occlusion = g_settings.occlusion;

			SEA_LOG("Config occlusion: Enabled=%d Wall=%.1fdB Max=%.1fdB LFRatio=%.2f RoomRatio=%.2f MaxDistance=%.0f "
					"RayBudget=%d Refresh=%.2fs Attack=%.2fs Release=%.2fs BypassKey=0x%X",
				occlusion.enabled, occlusion.wallLevel, occlusion.maxOcclusion, occlusion.lfRatio, occlusion.roomRatio,
				occlusion.maxDistance, occlusion.rayBudget, occlusion.refreshInterval, occlusion.attackTime,
				occlusion.releaseTime, occlusion.bypassKey);

			const char* forceEnvironmentName = "off";

			if (debug.forceEnvironment) {
				forceEnvironmentName = engine::EnvironmentTypeName(debug.forceEnvironment);
			}

			SEA_LOG("Config debug: Force=%s Readback=%u LogSoundPlay=%d LogSoundEnvironment=%d LayoutProbeCount=%u "
					"DeferEaxSets=%d LogRouteTiming=%d LogVoiceCover=%d",
				forceEnvironmentName, debug.readbackCount, debug.logSoundPlay, debug.logSoundEnvironment,
				debug.layoutProbeCount, debug.deferEaxSets, debug.logRouteTiming, debug.logVoiceCover);

			char layerText[64] = "";
			std::size_t layerTextLength = 0;

			for (const std::uint8_t layer : debug.rayProbeLayers) {
				const int written = std::snprintf(layerText + layerTextLength, sizeof(layerText) - layerTextLength, " %u", layer);

				if (written < 0 || layerTextLength + written >= sizeof(layerText)) {
					break;
				}

				layerTextLength += written;
			}

			SEA_LOG("Config occlusion probes: RayProbeKey=0x%X Layers=[%s ] Range=%.0f SoundRequests=%d SoundUpdate=%d "
					"TestOcclusion=%d mB TestKey=0x%X Grid=%d PickThreads=%d LogOcclusion=%d",
				debug.rayProbeKey, layerText, debug.rayProbeRange, debug.probeSoundRequests, debug.probeSoundUpdate,
				debug.testOcclusion, debug.occlusionTestKey, debug.probeGrid, debug.probePickThreads, debug.logOcclusion);
		}

	}

	// Reads the INI at `iniPath` into settings and logs them. A missing key keeps the default.
	//
	// Thread: Main (load time only)
	void Load(const std::string& iniPath) {
		LoadReverb(iniPath.c_str());
		LoadSends(iniPath.c_str());
		LoadVoiceFilters(iniPath.c_str());
		LoadOcclusion(iniPath.c_str());
		LoadDistance(iniPath.c_str());
		LoadFixes(iniPath.c_str());
		LoadDebug(iniPath.c_str());

		SEA_LOG("Config loaded from '%s'.", iniPath.c_str());
		LogSettings();
	}

	// Returns settings as read by `Load`. Not changed after load.
	//
	// Thread: Any
	const Settings& Get() {
		return g_settings;
	}

}
