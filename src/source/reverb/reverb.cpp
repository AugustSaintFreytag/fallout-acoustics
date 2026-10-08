#include "reverb/reverb.h"

#include "audio/directsound.h"
#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "debug/eax_readback.h"
#include "engine/addresses.h"
#include "engine/environment.h"
#include "reverb/listener.h"
#include "reverb/presets.h"
#include "reverb/routing.h"
#include "occlusion/apply.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace sea::reverb {

	using mem::Field;

	namespace {

		// Thread: Main (Read, Write)
		// Thread: Audio (Read)
		std::atomic<bool> g_bypass{false};

		// Thread: Audio
		enum class EaxState {
			Unknown, Available, Unavailable
		};
		
		EaxState g_eaxState = EaxState::Unknown;
		std::uint32_t g_appliedEnvironment = 0;
		LONG g_appliedVolume = 1;  // Invalid level, so the first `ApplyVolume` always sets the volume

		// Adds the deferred flag if `[Debug] bDeferEaxSets` is on.
		// The last set in `ApplySource` is immediate and commits all deferred values.
		// If a set fails before it, the next sound commits the values instead.
		ULONG Deferred(ULONG propertyId) {
			if (!config::Get().debug.deferEaxSets) {
				return propertyId;
			}

			return propertyId | eax::kDeferred;
		}

		// Loads the preset of the listener environment (or `[Debug] sForceEnvironment`) into FX slot 0, if it changed.
		// Adds `fRoomBoost` to the room level of the preset.
		// Returns true if the reverb parameters changed.
		bool ApplyEnvironment(IKsPropertySet* propertySet) {
			const config::Settings& settings = config::Get();
			std::uint32_t environment = GetListenerEnvironment();

			if (settings.debug.forceEnvironment) {
				environment = settings.debug.forceEnvironment;
			}

			if (environment == 0 || environment == g_appliedEnvironment) {
				return false;
			}

			eax::ReverbProperties preset = kPresets[environment - 1];
			const LONG roomBoost = static_cast<LONG>(std::lround(settings.reverb.roomBoost * 100.0f));
			preset.room = std::clamp(preset.room + roomBoost, eax::kMinLevel, 0L);

			const ULONG propertyId = Deferred(eax::kReverb_AllParameters);

			if (!eax::SetProperty(propertySet, eax::kFXSlot0, propertyId, &preset, sizeof(preset), "reverb parameters")) {
				return false;
			}

			g_appliedEnvironment = environment;

			const char* forcedSuffix = "";

			if (settings.debug.forceEnvironment) {
				forcedSuffix = " [Forced]";
			}

			SEA_LOG("[Reverb] Slot 0 <- %s%s (Decay %.2fs, Room %ld mB)", engine::EnvironmentTypeName(environment),
				forcedSuffix, preset.decayTime, preset.room);

			return true;
		}

		// Raises the direct level of a radio placed in the world by `[Reverb] fRadioBoost`.
		// The set is deferred if `[Debug] bDeferEaxSets` is on. The last set in `ApplySource` commits it.
		void ApplyRadioBoost(IKsPropertySet* propertySet, std::uint32_t soundFlags) {
			const float boost = config::Get().reverb.radioBoost;

			if (boost <= 0.0f || !IsWorldRadio(soundFlags)) {
				return;
			}

			// OpenAL Soft lets the direct level go above 0 dB, up to +10 dB.
			// Reference at "al/source.cpp", "eax_create_direct_filter_param".
			const LONG direct = std::min(static_cast<LONG>(std::lround(boost * 100.0f)), eax::kMaxDirect);
			eax::SetProperty(propertySet, eax::kSource, Deferred(eax::kSource_Direct), &direct, sizeof(direct), "radio boost");
		}

		// Sets the primary FX slot volume to the configured wet level or to silent while bypassed. 
		// Bails if volume is unchanged.
		void ApplyVolume(IKsPropertySet* propertySet) {
			LONG volume = eax::DecibelsToMillibels(config::Get().reverb.wetLevel);

			if (g_bypass.load(std::memory_order_relaxed)) {
				volume = eax::kMinLevel;
			}

			if (volume == g_appliedVolume) {
				return;
			}

			if (!eax::SetProperty(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume), "slot volume")) {
				return;
			}

			g_appliedVolume = volume;

			if (volume == eax::kMinLevel) {
				SEA_LOG("[Reverb] Slot 0 Volume %ld mB (Bypass)", volume);
			} else {
				SEA_LOG("[Reverb] Slot 0 Volume %ld mB", volume);
			}
		}

		// Sets FX slot 0 to the listener environment and the wet level.
		// Returns true if reverb parameters changed.
		bool ApplySlot(IKsPropertySet* propertySet) {
			const bool environmentChanged = ApplyEnvironment(propertySet);
			ApplyVolume(propertySet);

			return environmentChanged;
		}

		// Sets the active FX slots of a sound and its send level into the primary fx slot. 
		// A sound with its send set to off (muted) does not get a slot.
		//
		// `keepSlot`: the sound can be occluded. OpenAL Soft applies source occlusion to the direct path only while
		// slot 0 is active, so such a sound keeps slot 0 with a silent send instead of null slots.
		void ApplySource(IKsPropertySet* propertySet, const Route& route, bool keepSlot) {
			eax::ActiveFXSlots slots{};

			if (route.sendLevel <= config::kSendOff && !keepSlot) {
				eax::SetProperty(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots), "active slots (none)");

				return;
			}

			slots.slots[0] = eax::kFXSlot0;

			const ULONG propertyId = Deferred(eax::kSource_ActiveFXSlotID);

			if (!eax::SetProperty(propertySet, eax::kSource, propertyId, &slots, sizeof(slots), "active slots")) {
				return;
			}

			const eax::SourceSendProperties send{eax::kFXSlot0, eax::DecibelsToMillibels(route.sendLevel), 0};  // Off: -10000 mB
			eax::SetProperty(propertySet, eax::kSource, eax::kSource_SendParameters, &send, sizeof(send), "send level");
		}

		// Checks whether EAX is available by trying a property set on the primary FX slot.
		// Marks EAX available or unavailable for the session.
		//
		// EAX 4 slots 0 and 1 are locked legacy slots (OpenAL Soft: eax4_fx_slot_ensure_unlocked).
		// Slot 0 always holds a reverb, and LOADEFFECT on it fails.
		bool ProbeEax(IKsPropertySet* propertySet) {
			const LONG volume = eax::kMinLevel;

			if (!eax::SetProperty(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume), "probe slot 0 volume")) {
				g_eaxState = EaxState::Unavailable;
				SEA_LOG("[Reverb] EAX is unavailable, reverb is disabled. Is DSOAL installed?");

				return false;
			}

			g_eaxState = EaxState::Available;
			g_appliedVolume = volume;  // Muted until ApplySlot sets configured level.

			SEA_LOG("[Reverb] EAX is available (FX slot 0 reverb).");

			return true;
		}

	}

	// Toggles the bypass for reverb processed audio in the final mix. Effectively mutes "wet" output.
	// Returns true if processing is active (not bypassed/not disabled) after the toggle.
	// 
	// Thread: Main
	bool ToggleBypass() {
		const bool bypass = !g_bypass.load();
		g_bypass.store(bypass);

		if (bypass) {
			SEA_LOG("[Reverb] Bypass is on. Output is dry.");
		} else {
			SEA_LOG("[Reverb] Bypass is off. Reverb is active.");
		}

		return !bypass;
	}

	// Returns if reverb processed audio is currently bypassed for the final mix.
	// 
	// Thread: Any
	bool IsBypassed() {
		return g_bypass.load(std::memory_order_relaxed);
	}

	// Sets up reverb for the given sound. Updates primary FX slot, then the sound's active slot and send level.
	// Called before the original `Play`.
	// Returns the routing label.
	// 
	// Thread: Audio
	const char* OnSoundPlay(void* gameSound) {
		if (!config::Get().reverb.enabled) {
			return "disabled";
		}

		if (g_eaxState == EaxState::Unavailable) {
			return "no-eax";
		}

		const std::uint32_t buffer = Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer);

		if (!audio::IsDSoundObject(buffer)) {
			return "no-buffer";
		}

		IKsPropertySet* propertySet = nullptr;
		const HRESULT result = reinterpret_cast<IUnknown*>(buffer)->QueryInterface(IID_IKsPropertySet,
			reinterpret_cast<void**>(&propertySet));

		if (FAILED(result) || !propertySet) {
			return "no-propset";
		}

		if (g_eaxState == EaxState::Unknown && !ProbeEax(propertySet)) {
			propertySet->Release();

			return "no-eax";
		}

		const bool environmentChanged = ApplySlot(propertySet);

		const std::uint32_t soundFlags = Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags);
		const Route route = Classify(soundFlags);
		const bool keepSlot = config::Get().occlusion.enabled && occlusion::HasPosition(gameSound);
		ApplyRadioBoost(propertySet, soundFlags);
		ApplySource(propertySet, route, keepSlot);

		// Read back after ApplySource so deferred values can be committed.
		if (environmentChanged) {
			debug::ReadBackSlot(propertySet);
		}

		if (route.sendLevel > config::kSendOff) {
			debug::ReadBackSource(propertySet, route.label);
		}

		propertySet->Release();

		return route.label;
	}

}
