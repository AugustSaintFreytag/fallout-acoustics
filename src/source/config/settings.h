#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sea::config {

	// Send level in dB at and below which a sound gets no FX slot.
	constexpr float kSendOff = -100.0f; 

	// Properties for global reverb handling.
	struct ReverbSettings {
		// Toggles all reverb processing.
		bool enabled = true;

		// Final output of all reverb (in dB).
		// Internally corresponds to the output volume of FX slot 0.
		float wetLevel = 0.0f;

		// Volume boost added to the room level of each preset (in dB). (Max: +10.0f)
		float roomBoost = 0.0f;

		std::uint32_t interiorFallback = 26;  // ANAM for an interior without an acoustic space (MediumRoom)
		std::uint32_t exteriorFallback = 18;  // ANAM for an exterior without an acoustic space (City)
	};

	// Properties for reverb level modifiers for sound categories (in dB).
	struct SendSettings {
		float voice3D = 0.0f;
		float voice2D = -3.0f;
		float weapons = 0.0f;
		float footsteps = -6.0f;
		float loops3D = -6.0f;
		float loops2D = kSendOff;
		float region = kSendOff;
		float radio3D = 0.0f;
		float default3D = 0.0f;
		float default2D = 0.0f;
	};

	// Properties for filtering voices (cloth masks, gas masks, power armor helmets, intercoms).
	struct VoiceFilterSettings {
		bool enabled = true;
	};

	// Properties for sound occlusion values.
	struct OcclusionSettings {
		bool enabled = false;

		// High-frequency attenuation for each occluder between listener and sound, and the limit, in dB.
		// EAX applies a quarter of it to the low frequencies (fLFRatio) and 1.5 times to the reverb send (fRoomRatio).
		float wallLevel = 15.0f;
		float maxOcclusion = 60.0f;
		float lfRatio = 0.25f;
		float roomRatio = 1.5f;

		float maxDistance = 4096.0f;  // Game units. Sounds further away are not probed.
		int rayBudget = 64;  // Casts per frame
		float refreshInterval = 0.1f;  // Seconds between probes of the same sound

		// Seconds for a change over the full range (0 to fMaxOcclusion). Attack = more occlusion, release = less.
		float attackTime = 0.06f;
		float releaseTime = 0.25f;

		int bypassKey = 0;  // Virtual-key code that turns occlusion off and on, 0 = no key
	};

	// Properties for engine behavior fixes.
	struct FixSettings {
		// Mutes playback of 2D open/close sounds for doors and containers that are assumed to have animation-driven sounds.
		bool openCloseSounds = true;
	};

	// Properties for hotkeys used in debugging.
	struct HotkeySettings {
		// Virtual-key code (VK_END), 0 = no key
		int bypassKey = 0x23;
	};

	// Properties for debugging.
	struct DebugSettings {
		// ANAM to force use in all locations (0 = off).
		std::uint32_t forceEnvironment = 0;

		// Logs slot and source state from OpenAL Soft for the first n routed sounds.
		std::uint32_t readbackCount = 0;

		// Logs all sounds played by the engine.
		bool logSoundPlay = true;

		// Logs every sound environment change.
		bool logSoundEnvironment = true;

		// Scans the first n played sounds for DirectSound COM pointers.
		std::uint32_t layoutProbeCount = 32;

		// Sends reverb and source properties of a sound as deferred sets.
		// The last set of each sound commits all of them at one time.
		// EAX feature. Side effects not verified.
		bool deferEaxSets = false;

		// Logs time spent in reverb routing for each sound. Summary printed every 500 sounds.
		bool logRouteTiming = false;

		// Logs face cover of each speaking actor, with worn items in head equipment slots.
		// Face cover detection is used to determine pre-filters for voice modulation.
		bool logVoiceCover = true;

		// Key used to cast rays from the camera along its view and log hits (0 to disable).
		int rayProbeKey = 0;

		// Ray layers to test when casting.
		std::vector<std::uint8_t> rayProbeLayers{37, 1, 6};
		
		// Maximum length of rays (in game units).
		float rayProbeRange = 4096.0f;

		// Hooks `BSSoundHandle` play and position functions. Logs request threads and delay until playback.
		bool probeSoundRequests = false;

		// Hooks `BSWin32GameSound::Update`. Logs the threads it runs on, intervals and emitter positions.
		bool probeSoundUpdate = false;

		// Forces occlusion on all 3D sounds, to test which sounds get occlusion at all.
		// Value is EAX occlusion in mB for all 3D sounds (0 = off). 
		int testOcclusion = 0;

		// Key to toggle the test occlusion while sounds play (0 = no key).
		int occlusionTestKey = 0;

		// Logs static architecture grid alignment in interior cells.
		// The idea is that most Gamebryo interiors are made of chunky statics that can 
		// make the placement of walls predictable.
		bool probeGrid = false;

		// Hooks `bhkWorld::PickObject`. Logs which threads the engine uses to cast its own rays from 
		// to run the "what am I looking at" check. Summary every 30 seconds.
		bool probePickThreads = false;

		// Logs each change of a sound's occlusion target, with the objects the rays hit.
		bool logOcclusion = false;
	};

	struct Settings {
		ReverbSettings reverb;
		SendSettings sends;
		VoiceFilterSettings voiceFilters;
		OcclusionSettings occlusion;
		FixSettings fixes;
		HotkeySettings hotkeys;
		DebugSettings debug;
	};

	void Load(const std::string& iniPath);

	const Settings& Get();

}
