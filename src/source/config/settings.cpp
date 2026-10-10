#include "config/settings.h"

#include "engine/environment.h"
#include "utils/log.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace sea::config {

	namespace {

		// Settings before the first `Load`.
		const Settings g_defaultSettings;

		// Thread: Main (Read, Write)
		// Thread: Any (Read)
		std::atomic<const Settings*> g_currentSettings{&g_defaultSettings};

		// All loaded settings, kept until exit. Another thread can still read the previous settings during a reload.
		// Thread: Main
		std::vector<std::unique_ptr<Settings>> g_loadedSettings;
		std::string g_iniPath;

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

		void LoadReverb(const char* iniPath, Settings& settings) {
			ReverbSettings& reverb = settings.reverb;

			reverb.enabled = ReadBool(iniPath, "Reverb", "bEnabled", reverb.enabled);
			reverb.wetLevel = ReadFloat(iniPath, "Reverb", "fWetLevel", reverb.wetLevel);
			reverb.roomBoost = ReadFloat(iniPath, "Reverb", "fRoomBoost", reverb.roomBoost);
			reverb.radioBoost = ReadFloat(iniPath, "Reverb", "fRadioBoost", reverb.radioBoost);
			reverb.interiorFallback = ReadEnvironment(iniPath, "Reverb", "sInteriorFallback", reverb.interiorFallback);
			reverb.exteriorFallback = ReadEnvironment(iniPath, "Reverb", "sExteriorFallback", reverb.exteriorFallback);

			// The reverb bypass key is in `[Reverb]`, but is stored with the other hotkeys.
			HotkeySettings& hotkeys = settings.hotkeys;
			hotkeys.bypassKey = ReadInt(iniPath, "Reverb", "iBypassKey", hotkeys.bypassKey);
		}

		void LoadSends(const char* iniPath, Settings& settings) {
			SendSettings& sends = settings.sends;

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

		void LoadVoiceFilters(const char* iniPath, Settings& settings) {
			VoiceFilterSettings& voiceFilters = settings.voiceFilters;

			voiceFilters.enabled = ReadBool(iniPath, "VoiceFilters", "bEnabled", voiceFilters.enabled);
		}

		void LoadGunshots(const char* iniPath, Settings& settings) {
			GunshotSettings& gunshots = settings.gunshots;

			gunshots.deverb = ReadBool(iniPath, "Gunshots", "bDeverb", gunshots.deverb);

			const float keepFraction = ReadFloat(iniPath, "Gunshots", "fKeepFraction", gunshots.keepFraction);
			gunshots.keepFraction = std::clamp(keepFraction, 0.05f, 1.0f);

			const float fadeThreshold = ReadFloat(iniPath, "Gunshots", "fFadeThreshold", gunshots.fadeThreshold);
			gunshots.fadeThreshold = std::max(fadeThreshold, 0.0f);

			const float maxFadeDelay = ReadFloat(iniPath, "Gunshots", "fMaxFadeDelay", gunshots.maxFadeDelay);
			gunshots.maxFadeDelay = std::max(maxFadeDelay, 0.0f);

			const float decayRate = ReadFloat(iniPath, "Gunshots", "fDecayRate", gunshots.decayRate);
			gunshots.decayRate = std::max(decayRate, 1.0f);

			const float repeatLevel = ReadFloat(iniPath, "Gunshots", "fRepeatLevel", gunshots.repeatLevel);
			gunshots.repeatLevel = std::max(repeatLevel, 0.0f);

			gunshots.exteriorTail = ReadBool(iniPath, "Gunshots", "bExteriorTail", gunshots.exteriorTail);

			const float tailDecayFactor = ReadFloat(iniPath, "Gunshots", "fTailDecayFactor", gunshots.tailDecayFactor);
			gunshots.tailDecayFactor = std::clamp(tailDecayFactor, 0.1f, 10.0f);

			const float tailReflectionsDelay = ReadFloat(iniPath, "Gunshots", "fTailReflectionsDelay", gunshots.tailReflectionsDelay);
			gunshots.tailReflectionsDelay = std::clamp(tailReflectionsDelay, 0.0f, 0.3f);

			const float tailDiffusionFactor = ReadFloat(iniPath, "Gunshots", "fTailDiffusionFactor", gunshots.tailDiffusionFactor);
			gunshots.tailDiffusionFactor = std::clamp(tailDiffusionFactor, 0.0f, 10.0f);

			const float tailHFRatioFactor = ReadFloat(iniPath, "Gunshots", "fTailHFRatioFactor", gunshots.tailHFRatioFactor);
			gunshots.tailHFRatioFactor = std::clamp(tailHFRatioFactor, 0.0f, 10.0f);

			gunshots.tailLateLevel = ReadFloat(iniPath, "Gunshots", "fTailLateLevel", gunshots.tailLateLevel);
			gunshots.tailNearSend = ReadFloat(iniPath, "Gunshots", "fTailNearSend", gunshots.tailNearSend);
			gunshots.tailFarSend = ReadFloat(iniPath, "Gunshots", "fTailFarSend", gunshots.tailFarSend);

			const float tailNearDistance = ReadFloat(iniPath, "Gunshots", "fTailNearDistance", gunshots.tailNearDistance);
			gunshots.tailNearDistance = std::max(tailNearDistance, 0.0f);

			const float tailFarDistance = ReadFloat(iniPath, "Gunshots", "fTailFarDistance", gunshots.tailFarDistance);
			gunshots.tailFarDistance = std::max(tailFarDistance, gunshots.tailNearDistance + 1.0f);
		}

		void LoadOcclusion(const char* iniPath, Settings& settings) {
			OcclusionSettings& occlusion = settings.occlusion;

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

		void LoadDistance(const char* iniPath, Settings& settings) {
			DistanceSettings& distance = settings.distance;

			const float factor = ReadFloat(iniPath, "Distance", "fDistanceAttenuationFactor", distance.attenuationFactor);
			distance.attenuationFactor = std::clamp(factor, 0.1f, 10.0f);
		}

		void LoadFixes(const char* iniPath, Settings& settings) {
			FixSettings& fixes = settings.fixes;

			fixes.openCloseSounds = ReadBool(iniPath, "Fixes", "bFixDoubleOpenCloseSounds", fixes.openCloseSounds);
		}

		void LoadDebug(const char* iniPath, Settings& settings) {
			DebugSettings& debug = settings.debug;

			debug.forceEnvironment = ReadEnvironment(iniPath, "Debug", "sForceEnvironment", debug.forceEnvironment);
			debug.readbackCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iReadbackCount", debug.readbackCount));
			debug.logSoundPlay = ReadBool(iniPath, "Debug", "bLogSoundPlay", debug.logSoundPlay);
			debug.logSoundEnvironment = ReadBool(iniPath, "Debug", "bLogSoundEnvironment", debug.logSoundEnvironment);
			debug.layoutProbeCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iLayoutProbeCount", debug.layoutProbeCount));
			debug.deferEaxSets = ReadBool(iniPath, "Debug", "bDeferEaxSets", debug.deferEaxSets);
			debug.logRouteTiming = ReadBool(iniPath, "Debug", "bLogRouteTiming", debug.logRouteTiming);
			debug.logVoiceCover = ReadBool(iniPath, "Debug", "bLogVoiceCover", debug.logVoiceCover);
			debug.logGunshots = ReadBool(iniPath, "Debug", "bLogGunshots", debug.logGunshots);

			// The reload key is in `[Debug]`, but is stored with the other hotkeys.
			HotkeySettings& hotkeys = settings.hotkeys;
			hotkeys.reloadKey = ReadInt(iniPath, "Debug", "iReloadKey", hotkeys.reloadKey);

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

		void LogSettings(const Settings& settings) {
			const ReverbSettings& reverb = settings.reverb;
			const SendSettings& sends = settings.sends;
			const DebugSettings& debug = settings.debug;

			SEA_LOG("Config reverb: Enabled=%d Wet=%.1fdB RoomBoost=%.1fdB RadioBoost=%.1fdB InteriorFallback=%s "
					"ExteriorFallback=%s BypassKey=0x%X",
				reverb.enabled, reverb.wetLevel, reverb.roomBoost, reverb.radioBoost, engine::EnvironmentTypeName(reverb.interiorFallback),
				engine::EnvironmentTypeName(reverb.exteriorFallback), settings.hotkeys.bypassKey);

			SEA_LOG("Config sends (dB): Voice3D=%.1f Voice2D=%.1f Weapons=%.1f Footsteps=%.1f Loops3D=%.1f Loops2D=%.1f "
					"Region=%.1f Radio3D=%.1f Default3D=%.1f Default2D=%.1f",
				sends.voice3D, sends.voice2D, sends.weapons, sends.footsteps, sends.loops3D, sends.loops2D, sends.region,
				sends.radio3D, sends.default3D, sends.default2D);

			SEA_LOG("Config voice filters: Enabled=%d", settings.voiceFilters.enabled);

			const GunshotSettings& gunshots = settings.gunshots;

			SEA_LOG("Config gunshots: Deverb=%d Keep=%.2f FadeThreshold=%.1fdB MaxFadeDelay=%.3fs Decay=%.0fdB/s "
					"RepeatLevel=%.1fdB",
				gunshots.deverb, gunshots.keepFraction, gunshots.fadeThreshold, gunshots.maxFadeDelay,
				gunshots.decayRate, gunshots.repeatLevel);

			SEA_LOG("Config gunfire tail: Enabled=%d DecayFactor=%.2f ReflectionsDelay=+%.3fs DiffusionFactor=%.2f "
					"HFRatioFactor=%.2f LateLevel=%+.1fdB Send=%.1fdB..%.1fdB Distance=%.0f..%.0f",
				gunshots.exteriorTail, gunshots.tailDecayFactor, gunshots.tailReflectionsDelay, gunshots.tailDiffusionFactor,
				gunshots.tailHFRatioFactor, gunshots.tailLateLevel, gunshots.tailNearSend, gunshots.tailFarSend,
				gunshots.tailNearDistance, gunshots.tailFarDistance);

			SEA_LOG("Config distance: AttenuationFactor=%.2f", settings.distance.attenuationFactor);
			SEA_LOG("Config fixes: OpenCloseSounds=%d", settings.fixes.openCloseSounds);

			const OcclusionSettings& occlusion = settings.occlusion;

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
					"DeferEaxSets=%d LogRouteTiming=%d LogVoiceCover=%d LogGunshots=%d ReloadKey=0x%X",
				forceEnvironmentName, debug.readbackCount, debug.logSoundPlay, debug.logSoundEnvironment,
				debug.layoutProbeCount, debug.deferEaxSets, debug.logRouteTiming, debug.logVoiceCover,
				debug.logGunshots, settings.hotkeys.reloadKey);

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

		void ReadSettings(const char* iniPath, Settings& settings) {
			LoadReverb(iniPath, settings);
			LoadSends(iniPath, settings);
			LoadVoiceFilters(iniPath, settings);
			LoadGunshots(iniPath, settings);
			LoadOcclusion(iniPath, settings);
			LoadDistance(iniPath, settings);
			LoadFixes(iniPath, settings);
			LoadDebug(iniPath, settings);
		}

		// Makes the given settings current and keeps them until exit.
		void Publish(std::unique_ptr<Settings> settings) {
			g_currentSettings.store(settings.get(), std::memory_order_release);
			g_loadedSettings.push_back(std::move(settings));
		}

		// A setting that decides at load which hooks get installed. A reload cannot change it.
		struct LoadTimeSwitch {
			const char* key;
			bool previous;
			bool next;
		};

		// Logs each load-time switch that differs between the given previous and next settings.
		void LogLoadTimeChanges(const Settings& previous, const Settings& next) {
			const LoadTimeSwitch switches[] = {
				{"[VoiceFilters] bEnabled", previous.voiceFilters.enabled, next.voiceFilters.enabled},
				{"[Occlusion] bEnabled", previous.occlusion.enabled, next.occlusion.enabled},
				{"[Fixes] bFixDoubleOpenCloseSounds", previous.fixes.openCloseSounds, next.fixes.openCloseSounds},
				{"[Debug] bLogSoundEnvironment", previous.debug.logSoundEnvironment, next.debug.logSoundEnvironment},
				{"[Debug] bProbeSoundRequests", previous.debug.probeSoundRequests, next.debug.probeSoundRequests},
				{"[Debug] bProbeSoundUpdate", previous.debug.probeSoundUpdate, next.debug.probeSoundUpdate},
				{"[Debug] iTestOcclusion", previous.debug.testOcclusion != 0, next.debug.testOcclusion != 0},
				{"[Debug] bProbePickThreads", previous.debug.probePickThreads, next.debug.probePickThreads},
			};

			for (const LoadTimeSwitch& loadTimeSwitch : switches) {
				if (loadTimeSwitch.previous != loadTimeSwitch.next) {
					SEA_LOG("Config: %s changed. It needs a restart to apply.", loadTimeSwitch.key);
				}
			}
		}

	}

	// Reads the INI at the given path into new settings, makes them current and logs them. A missing key keeps the default.
	// Remembers the path for `Reload`.
	//
	// Thread: Main (load time only)
	void Load(const std::string& iniPath) {
		g_iniPath = iniPath;

		auto settings = std::make_unique<Settings>();
		ReadSettings(g_iniPath.c_str(), *settings);

		SEA_LOG("Config loaded from '%s'.", g_iniPath.c_str());
		LogSettings(*settings);
		Publish(std::move(settings));
	}

	// Reads the INI from `Load` again into new settings, makes them current and logs them.
	// Values read on use apply at once. Reverb presets apply on the next sound.
	// Load-time switches keep their effect until a restart. Each changed one is logged.
	//
	// Thread: Main
	void Reload() {
		auto settings = std::make_unique<Settings>();
		ReadSettings(g_iniPath.c_str(), *settings);

		SEA_LOG("Config reloaded from '%s'.", g_iniPath.c_str());
		LogLoadTimeChanges(Get(), *settings);
		LogSettings(*settings);
		Publish(std::move(settings));
	}

	// Returns the current settings. A reload replaces them, but the returned settings stay valid until exit.
	//
	// Thread: Any
	const Settings& Get() {
		return *g_currentSettings.load(std::memory_order_acquire);
	}

}
