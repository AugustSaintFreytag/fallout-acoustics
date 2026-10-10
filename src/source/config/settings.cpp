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

		// Reads a level in dB for a whole source, clamped to the EAX source range [-100, +10].
		float ReadSourceLevel(const char* iniPath, const char* key, float fallback) {
			return std::clamp(ReadFloat(iniPath, "Sources", key, fallback), -100.0f, 10.0f);
		}

		void LoadSources(const char* iniPath, Settings& settings) {
			SourceSettings& sources = settings.sources;

			sources.radioLevel = ReadSourceLevel(iniPath, "fRadioLevel", sources.radioLevel);
			sources.ambienceLevel = ReadSourceLevel(iniPath, "fAmbienceLevel", sources.ambienceLevel);

			const float factor = ReadFloat(iniPath, "Sources", "fDistanceAttenuationFactor", sources.attenuationFactor);
			sources.attenuationFactor = std::clamp(factor, 0.1f, 10.0f);
		}

		void LoadSpatialization(const char* iniPath, Settings& settings) {
			SpatializationSettings& spatialization = settings.spatialization;
			const char* section = "Spatialization";

			spatialization.enabled = ReadBool(iniPath, section, "bEnabled", spatialization.enabled);
			spatialization.wetLevel = ReadFloat(iniPath, section, "fWetLevel", spatialization.wetLevel);
			spatialization.bypassKey = ReadInt(iniPath, section, "iBypassKey", spatialization.bypassKey);
			spatialization.interiorFallback = ReadEnvironment(iniPath, section, "sInteriorFallback", spatialization.interiorFallback);
			spatialization.exteriorFallback = ReadEnvironment(iniPath, section, "sExteriorFallback", spatialization.exteriorFallback);

			SendLevels& sends = spatialization.sends;

			sends.voice3D = ReadFloat(iniPath, section, "fVoice3D", sends.voice3D);
			sends.voice2D = ReadFloat(iniPath, section, "fVoice2D", sends.voice2D);
			sends.weapons = ReadFloat(iniPath, section, "fWeapons", sends.weapons);
			sends.footsteps = ReadFloat(iniPath, section, "fFootsteps", sends.footsteps);
			sends.loops3D = ReadFloat(iniPath, section, "fLoops3D", sends.loops3D);
			sends.loops2D = ReadFloat(iniPath, section, "fLoops2D", sends.loops2D);
			sends.region = ReadFloat(iniPath, section, "fRegion", sends.region);
			sends.radio3D = ReadFloat(iniPath, section, "fRadio3D", sends.radio3D);
			sends.default3D = ReadFloat(iniPath, section, "fDefault3D", sends.default3D);
			sends.default2D = ReadFloat(iniPath, section, "fDefault2D", sends.default2D);
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

		void LoadImpacts(const char* iniPath, Settings& settings) {
			ImpactSettings& impacts = settings.impacts;

			impacts.deverb = ReadBool(iniPath, "Impacts", "bDeverb", impacts.deverb);

			const float keepFraction = ReadFloat(iniPath, "Impacts", "fKeepFraction", impacts.keepFraction);
			impacts.keepFraction = std::clamp(keepFraction, 0.05f, 1.0f);

			const float fadeThreshold = ReadFloat(iniPath, "Impacts", "fFadeThreshold", impacts.fadeThreshold);
			impacts.fadeThreshold = std::max(fadeThreshold, 0.0f);

			const float maxFadeDelay = ReadFloat(iniPath, "Impacts", "fMaxFadeDelay", impacts.maxFadeDelay);
			impacts.maxFadeDelay = std::max(maxFadeDelay, 0.0f);

			const float decayRate = ReadFloat(iniPath, "Impacts", "fDecayRate", impacts.decayRate);
			impacts.decayRate = std::max(decayRate, 1.0f);

			const float repeatRejectLevel = ReadFloat(iniPath, "Impacts", "fRepeatRejectLevel", impacts.repeatRejectLevel);
			impacts.repeatRejectLevel = std::max(repeatRejectLevel, 0.0f);

			impacts.exteriorTail = ReadBool(iniPath, "Impacts", "bExteriorTail", impacts.exteriorTail);

			const float tailDecayFactor = ReadFloat(iniPath, "Impacts", "fTailDecayFactor", impacts.tailDecayFactor);
			impacts.tailDecayFactor = std::clamp(tailDecayFactor, 0.1f, 10.0f);

			const float tailReflectionsDelay = ReadFloat(iniPath, "Impacts", "fTailReflectionsDelay", impacts.tailReflectionsDelay);
			impacts.tailReflectionsDelay = std::clamp(tailReflectionsDelay, 0.0f, 0.3f);

			const float tailDiffusionFactor = ReadFloat(iniPath, "Impacts", "fTailDiffusionFactor", impacts.tailDiffusionFactor);
			impacts.tailDiffusionFactor = std::clamp(tailDiffusionFactor, 0.0f, 10.0f);

			const float tailHFRatioFactor = ReadFloat(iniPath, "Impacts", "fTailHFRatioFactor", impacts.tailHFRatioFactor);
			impacts.tailHFRatioFactor = std::clamp(tailHFRatioFactor, 0.0f, 10.0f);

			impacts.tailLateLevel = ReadFloat(iniPath, "Impacts", "fTailLateLevel", impacts.tailLateLevel);
			impacts.tailNearSend = ReadFloat(iniPath, "Impacts", "fTailNearSend", impacts.tailNearSend);
			impacts.tailFarSend = ReadFloat(iniPath, "Impacts", "fTailFarSend", impacts.tailFarSend);

			const float tailNearDistance = ReadFloat(iniPath, "Impacts", "fTailNearDistance", impacts.tailNearDistance);
			impacts.tailNearDistance = std::max(tailNearDistance, 0.0f);

			const float tailFarDistance = ReadFloat(iniPath, "Impacts", "fTailFarDistance", impacts.tailFarDistance);
			impacts.tailFarDistance = std::max(tailFarDistance, impacts.tailNearDistance + 1.0f);
		}

		void LoadVocals(const char* iniPath, Settings& settings) {
			VocalSettings& vocals = settings.vocals;

			vocals.enabled = ReadBool(iniPath, "Vocals", "bEnabled", vocals.enabled);
		}

		void LoadFixes(const char* iniPath, Settings& settings) {
			FixSettings& fixes = settings.fixes;

			fixes.openCloseSounds = ReadBool(iniPath, "Fixes", "bFixDoubleOpenCloseSounds", fixes.openCloseSounds);
		}

		void LoadDebug(const char* iniPath, Settings& settings) {
			DebugSettings& debug = settings.debug;

			debug.reloadKey = ReadInt(iniPath, "Debug", "iReloadKey", debug.reloadKey);
			debug.forceEnvironment = ReadEnvironment(iniPath, "Debug", "sForceEnvironment", debug.forceEnvironment);
			debug.readbackCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iReadbackCount", debug.readbackCount));
			debug.logSoundPlay = ReadBool(iniPath, "Debug", "bLogSoundPlay", debug.logSoundPlay);
			debug.logSoundEnvironment = ReadBool(iniPath, "Debug", "bLogSoundEnvironment", debug.logSoundEnvironment);
			debug.layoutProbeCount = static_cast<std::uint32_t>(ReadInt(iniPath, "Debug", "iLayoutProbeCount", debug.layoutProbeCount));
			debug.deferEaxSets = ReadBool(iniPath, "Debug", "bDeferEaxSets", debug.deferEaxSets);
			debug.logRouteTiming = ReadBool(iniPath, "Debug", "bLogRouteTiming", debug.logRouteTiming);
			debug.logVoiceCover = ReadBool(iniPath, "Debug", "bLogVoiceCover", debug.logVoiceCover);
			debug.logGunshots = ReadBool(iniPath, "Debug", "bLogGunshots", debug.logGunshots);

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
			const SourceSettings& sources = settings.sources;

			SEA_LOG("Config sources: Radio=%.1fdB Ambience=%.1fdB AttenuationFactor=%.2f", sources.radioLevel,
				sources.ambienceLevel, sources.attenuationFactor);

			const SpatializationSettings& spatialization = settings.spatialization;
			const SendLevels& sends = spatialization.sends;

			SEA_LOG("Config spatialization: Enabled=%d Wet=%.1fdB BypassKey=0x%X InteriorFallback=%s ExteriorFallback=%s",
				spatialization.enabled, spatialization.wetLevel, spatialization.bypassKey,
				engine::EnvironmentTypeName(spatialization.interiorFallback),
				engine::EnvironmentTypeName(spatialization.exteriorFallback));

			SEA_LOG("Config sends (dB): Voice3D=%.1f Voice2D=%.1f Weapons=%.1f Footsteps=%.1f Loops3D=%.1f Loops2D=%.1f "
					"Region=%.1f Radio3D=%.1f Default3D=%.1f Default2D=%.1f",
				sends.voice3D, sends.voice2D, sends.weapons, sends.footsteps, sends.loops3D, sends.loops2D, sends.region,
				sends.radio3D, sends.default3D, sends.default2D);

			const OcclusionSettings& occlusion = settings.occlusion;

			SEA_LOG("Config occlusion: Enabled=%d Wall=%.1fdB Max=%.1fdB LFRatio=%.2f RoomRatio=%.2f MaxDistance=%.0f "
					"RayBudget=%d Refresh=%.2fs Attack=%.2fs Release=%.2fs BypassKey=0x%X",
				occlusion.enabled, occlusion.wallLevel, occlusion.maxOcclusion, occlusion.lfRatio, occlusion.roomRatio,
				occlusion.maxDistance, occlusion.rayBudget, occlusion.refreshInterval, occlusion.attackTime,
				occlusion.releaseTime, occlusion.bypassKey);

			const ImpactSettings& impacts = settings.impacts;

			SEA_LOG("Config impacts: Deverb=%d Keep=%.2f FadeThreshold=%.1fdB MaxFadeDelay=%.3fs Decay=%.0fdB/s "
					"RepeatLevel=%.1fdB",
				impacts.deverb, impacts.keepFraction, impacts.fadeThreshold, impacts.maxFadeDelay,
				impacts.decayRate, impacts.repeatRejectLevel);

			SEA_LOG("Config impacts tail: Enabled=%d DecayFactor=%.2f ReflectionsDelay=+%.3fs DiffusionFactor=%.2f "
					"HFRatioFactor=%.2f LateLevel=%+.1fdB Send=%.1fdB..%.1fdB Distance=%.0f..%.0f",
				impacts.exteriorTail, impacts.tailDecayFactor, impacts.tailReflectionsDelay, impacts.tailDiffusionFactor,
				impacts.tailHFRatioFactor, impacts.tailLateLevel, impacts.tailNearSend, impacts.tailFarSend,
				impacts.tailNearDistance, impacts.tailFarDistance);

			SEA_LOG("Config vocals: Enabled=%d", settings.vocals.enabled);
			SEA_LOG("Config fixes: OpenCloseSounds=%d", settings.fixes.openCloseSounds);

			const DebugSettings& debug = settings.debug;
			const char* forceEnvironmentName = "off";

			if (debug.forceEnvironment) {
				forceEnvironmentName = engine::EnvironmentTypeName(debug.forceEnvironment);
			}

			SEA_LOG("Config debug: ReloadKey=0x%X Force=%s Readback=%u LogSoundPlay=%d LogSoundEnvironment=%d "
					"LayoutProbeCount=%u DeferEaxSets=%d LogRouteTiming=%d LogVoiceCover=%d LogGunshots=%d",
				debug.reloadKey, forceEnvironmentName, debug.readbackCount, debug.logSoundPlay, debug.logSoundEnvironment,
				debug.layoutProbeCount, debug.deferEaxSets, debug.logRouteTiming, debug.logVoiceCover, debug.logGunshots);

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
			LoadSources(iniPath, settings);
			LoadSpatialization(iniPath, settings);
			LoadOcclusion(iniPath, settings);
			LoadImpacts(iniPath, settings);
			LoadVocals(iniPath, settings);
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
				{"[Vocals] bEnabled", previous.vocals.enabled, next.vocals.enabled},
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
