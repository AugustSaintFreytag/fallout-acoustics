#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sea::config {

	// Send level in dB at and below which a sound gets no FX slot.
	constexpr float kSendOff = -100.0f;

	// Properties for the level of whole sources, dry and reverb alike, and their distance attenuation.
	struct SourceSettings {
		// Levels in dB, [-100, +10]. 0 = unchanged.
		float radioLevel = 0.0f;  // Radios placed in the world
		float ambienceLevel = 0.0f;  // Region sounds and sounds in `sound\fx\amb\`
		float explosionsLevel = 0.0f;  // Sounds in `sound\fx\fx\explosion\`

		// Factor on the min and max attenuation distance of each 3D sound. 2.0 = sounds carry twice as far.
		float attenuationFactor = 1.0f;
	};

	// A set of reverb send levels for sound categories, in dB.
	struct SendLevels {
		float voice3D = 0.0f;
		float voice2D = -3.0f;
		float weapons = 0.0f;
		float footsteps = -6.0f;
		float loops3D = -6.0f;
		float loops2D = kSendOff;
		float region = kSendOff;
		float radio3D = 0.0f;
		float radio2D = kSendOff;  // Pip-Boy radio and holotapes
		float default3D = 0.0f;
		float default2D = 0.0f;
	};

	// Properties for reverb: listener environment, wet level and send levels.
	struct SpatializationSettings {
		// Toggles all reverb processing.
		bool enabled = true;

		// Final output of all reverb (in dB). Output volume of FX slots 0 and 2.
		float wetLevel = 0.0f;

		// Key that toggles the reverb bypass. Virtual-key code (VK_END), 0 = no key
		int bypassKey = 0x23;

		std::uint32_t interiorFallback = 26;  // ANAM for an interior without an acoustic space (MediumRoom)
		std::uint32_t exteriorFallback = 18;  // ANAM for an exterior without an acoustic space (City)

		SendLevels sends;
	};

	// Properties for sound occlusion values.
	struct OcclusionSettings {
		bool enabled = false;

		// High-frequency attenuation for each wall layer between listener and sound, and the limit, in dB.
		// EAX applies a quarter of it to the low frequencies (fLFRatio). The reverb send gets it times fRoomRatio.
		float wallLevel = 12.0f;
		float maxOcclusion = 60.0f;
		float lfRatio = 0.25f;
		float roomRatio = 1.0f;

		float maxDistance = 4096.0f;  // Game units. Sounds further away are not probed.
		int rayBudget = 64;  // Casts per frame
		float refreshInterval = 0.1f;  // Seconds between probes of the same sound

		// Seconds for a change over the full range (0 to fMaxOcclusion). Attack = more occlusion, release = less.
		float attackTime = 0.06f;
		float releaseTime = 0.25f;

		int bypassKey = 0;  // Virtual-key code that turns occlusion off and on, 0 = no key
	};

	// Properties for loud sounds: removing baked-in reverb from gunshots and the exterior tail for gunfire and explosions.
	struct ImpactSettings {
		bool deverb = false;

		float keepFraction = 0.5f;  // Of the sound length, (0, 1]
		float fadeThreshold = 3.0f;  // dB below the peak. Fade starts once the level stays below it.
		float maxFadeDelay = 0.25f;  // Seconds after the peak
		float decayRate = 120.0f;  // dB per second
		float repeatRejectLevel = 6.0f;  // dB below the peak. A later peak at this level counts as another shot.

		// Second reverb (FX slot 2) for gunfire and explosions in exteriors, derived from the listener environment preset.
		bool exteriorTail = false;
		float tailDecayFactor = 2.0f;
		float tailReflectionsDelay = 0.1f;  // Seconds, added
		float tailDiffusionFactor = 0.5f;
		float tailHFRatioFactor = 0.7f;
		float tailLateLevel = 12.0f;  // dB, added to the late reverb level

		// Send into the tail by distance between listener and sound. 2D sounds use the near level.
		float tailNearSend = -9.0f;  // dB
		float tailFarSend = 0.0f;  // dB
		float tailNearDistance = 1024.0f;  // Game units
		float tailFarDistance = 8192.0f;  // Game units
	};

	// Properties for filtering voices (cloth masks, gas masks, power armor helmets, intercoms) and holotapes.
	struct VocalSettings {
		bool enabled = true;

		// Filters holotape lines played by the Pip-Boy like playback from an old tape.
		bool holotapeFilter = true;
	};

	// Properties for engine behavior fixes.
	struct FixSettings {
		// Mutes playback of 2D open/close sounds for doors and containers that are assumed to have animation-driven sounds.
		bool openCloseSounds = true;

		// Mutes the Pip-Boy holotape start and stop sounds while a save loads and in the first second after.
		bool loadHolotapeSounds = true;

		// Seconds added to the total time the Pip-Boy shows for a holotape. [-10, +30]
		float holotapeDurationOffset = 0.0f;
	};

	// Properties for debugging.
	struct DebugSettings {
		// Key that reloads the INI. Virtual-key code (VK_INSERT), 0 = no key
		int reloadKey = 0x2D;

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

		// Logs each gunshot buffer once, deverbed or rejected, with the reason for a rejection.
		bool logGunshots = false;

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

	// All settings, one member for each INI section in file order.
	struct Settings {
		SourceSettings sources;
		SpatializationSettings spatialization;
		OcclusionSettings occlusion;
		ImpactSettings impacts;
		VocalSettings vocals;
		FixSettings fixes;
		DebugSettings debug;
	};

	void Load(const std::string& iniPath);

	void Reload();

	const Settings& Get();

}
