#include "reverb/impact_tail.h"

#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/environment.h"
#include "engine/sound_paths.h"
#include "occlusion/apply.h"
#include "reverb/listener.h"
#include "reverb/presets.h"
#include "reverb/reverb.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/vector3.h"

#include <algorithm>
#include <cmath>

namespace sea::reverb {

	using mem::Field;

	namespace {

		enum class SlotState {
			Unloaded,
			Loaded,
			Failed,
		};

		// Thread: Audio
		SlotState g_slotState = SlotState::Unloaded;
		eax::ReverbProperties g_appliedPreset{};  // Environment 0 = none applied yet
		LONG g_appliedVolume = 1;  // Invalid level, so the first `ApplyTailVolume` always sets the volume

		LONG DecibelsToLevel(float decibels) {
			return static_cast<LONG>(std::lround(decibels * 100.0f));
		}

		// Loads a reverb into FX slot 2 once per session.
		// Returns false if OpenAL Soft rejected it. The tail then stays off for the session.
		bool EnsureReverbLoaded(IKsPropertySet* propertySet) {
			if (g_slotState != SlotState::Unloaded) {
				return g_slotState == SlotState::Loaded;
			}

			if (!eax::SetProperty(propertySet, eax::kFXSlot2, eax::kFXSlot_LoadEffect, &eax::kReverbEffect, sizeof(GUID),
					"slot 2 load reverb")) {
				g_slotState = SlotState::Failed;
				SEA_LOG("[Tail] FX slot 2 does not accept a reverb. Impact tail is off.");

				return false;
			}

			g_slotState = SlotState::Loaded;
			SEA_LOG("[Tail] FX slot 2 holds a reverb for the impact tail.");

			return true;
		}

		// Returns the preset of the given environment, driven harder by the tail settings in `[Impacts]`.
		// All values are clamped to the EAX ranges.
		eax::ReverbProperties DeriveTailPreset(std::uint32_t environment) {
			const config::Settings& settings = config::Get();
			const config::ImpactSettings& impacts = settings.impacts;
			eax::ReverbProperties preset = kPresets[environment - 1];

			preset.decayTime = std::clamp(preset.decayTime * impacts.tailDecayFactor, eax::kMinDecayTime, eax::kMaxDecayTime);
			preset.decayHFRatio = std::clamp(preset.decayHFRatio * impacts.tailHFRatioFactor, eax::kMinDecayHFRatio,
				eax::kMaxDecayHFRatio);
			preset.environmentDiffusion = std::clamp(preset.environmentDiffusion * impacts.tailDiffusionFactor, 0.0f, 1.0f);
			preset.reflectionsDelay = std::min(preset.reflectionsDelay + impacts.tailReflectionsDelay, eax::kMaxReflectionsDelay);
			preset.reverb = std::clamp(preset.reverb + DecibelsToLevel(impacts.tailLateLevel), eax::kMinLevel, eax::kMaxReverb);

			return preset;
		}

		// Sets the slot 2 volume to the configured wet level, or to silent while bypassed.
		void ApplyTailVolume(IKsPropertySet* propertySet) {
			LONG volume = eax::DecibelsToMillibels(config::Get().spatialization.wetLevel);

			if (IsBypassed()) {
				volume = eax::kMinLevel;
			}

			if (volume == g_appliedVolume) {
				return;
			}

			if (eax::SetProperty(propertySet, eax::kFXSlot2, eax::kFXSlot_Volume, &volume, sizeof(volume), "slot 2 volume")) {
				g_appliedVolume = volume;
			}
		}

		// Returns the distance between listener and the given sound. Game units.
		// Returns 0 for a sound without position, like 1st-person gunfire.
		float DistanceToListener(void* gameSound) {
			if (!occlusion::HasPosition(gameSound)) {
				return 0.0f;
			}

			const Vector3 position = Field<Vector3>(gameSound, engine::kWin32Sound_EmitterPosition);

			// Position not set yet. Treat as near.
			if (position.x == 0.0f && position.y == 0.0f && position.z == 0.0f) {
				return 0.0f;
			}

			return Distance(GetListenerPosition(), position);
		}

	}

	// Loads the tail preset for the given environment into FX slot 2 and sets its volume.
	// Loads the reverb into the slot on first use. Sets the preset only if it differs from the applied one.
	// A changed setting after a reload counts as a difference.
	// Does nothing if `[Impacts] bExteriorTail` is off.
	//
	// Thread: Audio
	void ApplyTailSlot(IKsPropertySet* propertySet, std::uint32_t environment) {
		if (!config::Get().impacts.exteriorTail || environment == 0) {
			return;
		}

		if (!EnsureReverbLoaded(propertySet)) {
			return;
		}

		ApplyTailVolume(propertySet);

		const eax::ReverbProperties preset = DeriveTailPreset(environment);

		if (preset == g_appliedPreset) {
			return;
		}

		if (!eax::SetProperty(propertySet, eax::kFXSlot2, eax::kReverb_AllParameters, &preset, sizeof(preset),
				"slot 2 reverb parameters")) {
			return;
		}

		g_appliedPreset = preset;

		SEA_LOG("[Tail] Slot 2 <- %s tail (Decay %.2fs, Reflections Delay %.3fs, Diffusion %.2f, HF Ratio %.2f, "
				"Late %ld mB)",
			engine::EnvironmentTypeName(environment), preset.decayTime, preset.reflectionsDelay,
			preset.environmentDiffusion, preset.decayHFRatio, preset.reverb);
	}

	// Returns the send level into the impact tail (FX slot 2) for the given sound, in dB.
	// Only gunfire and explosions in exteriors get a level, shots and fire loops alike. Others get none.
	// The level rises from the near to the far send between the near and far distance.
	//
	// Thread: Audio
	std::optional<float> GetTailSendLevel(void* gameSound) {
		const config::Settings& settings = config::Get();
		const config::ImpactSettings& impacts = settings.impacts;

		if (!impacts.exteriorTail || g_slotState != SlotState::Loaded || !IsListenerInExterior()) {
			return std::nullopt;
		}

		const char* path = &Field<char>(gameSound, engine::kSound_FilePath);

		const bool isGunfire = engine::ClassifyGunfirePath(path) != engine::GunfireKind::None;

		if (!isGunfire && !engine::IsExplosionPath(path)) {
			return std::nullopt;
		}

		const float distance = DistanceToListener(gameSound);
		const float range = impacts.tailFarDistance - impacts.tailNearDistance;
		const float position = std::clamp((distance - impacts.tailNearDistance) / range, 0.0f, 1.0f);
		const float sendLevel = impacts.tailNearSend + (impacts.tailFarSend - impacts.tailNearSend) * position;

		if (settings.debug.logGunshots) {
			SEA_LOG("[Tail] %p at %.0f units, Send %.1f dB, Path=\"%.200s\"", gameSound, distance, sendLevel, path);
		}

		return sendLevel;
	}

}
