#include "reverb/reverb.h"

#include "audio/directsound.h"
#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "debug/eax_readback.h"
#include "engine/addresses.h"
#include "engine/environment.h"
#include "engine/holotapes.h"
#include "reverb/impact_tail.h"
#include "reverb/listener.h"
#include "reverb/presets.h"
#include "reverb/routing.h"
#include "occlusion/apply.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <optional>
#include <unordered_set>

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
		eax::ReverbProperties g_appliedPreset{};  // Environment 0 = none applied yet

		constexpr std::size_t kMaxLeveledBuffers = 1024;

		// Buffers that got a source level other than 0 dB. Only these need a reset to 0 dB on a later play.
		std::unordered_set<std::uintptr_t> g_leveledBuffers;
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

		// Returns the listener environment, or `[Debug] sForceEnvironment` if set. 0 = unknown.
		std::uint32_t ResolveEnvironment() {
			const std::uint32_t forceEnvironment = config::Get().debug.forceEnvironment;

			if (forceEnvironment) {
				return forceEnvironment;
			}

			return GetListenerEnvironment();
		}

		// Loads the preset of the given environment into FX slot 0, if it differs from the applied one.
		// Returns true if the reverb parameters changed.
		bool ApplyEnvironment(IKsPropertySet* propertySet, std::uint32_t environment) {
			const config::Settings& settings = config::Get();

			if (environment == 0) {
				return false;
			}

			const eax::ReverbProperties& preset = kPresets[environment - 1];

			if (preset == g_appliedPreset) {
				return false;
			}

			const ULONG propertyId = Deferred(eax::kReverb_AllParameters);

			if (!eax::SetProperty(propertySet, eax::kFXSlot0, propertyId, &preset, sizeof(preset), "reverb parameters")) {
				return false;
			}

			g_appliedPreset = preset;

			const char* forcedSuffix = "";

			if (settings.debug.forceEnvironment) {
				forcedSuffix = " [Forced]";
			}

			SEA_LOG("[Reverb] Slot 0 <- %s%s (Decay %.2fs, Room %ld mB)", engine::EnvironmentTypeName(environment),
				forcedSuffix, preset.decayTime, preset.room);

			return true;
		}

		// Sets the direct and room level of a sound to its level from `[Sources]`, so dry and reverb change alike.
		// Skips the sets for 0 dB unless the buffer got another level before. A reused buffer keeps its last level.
		// The sets are deferred if `[Debug] bDeferEaxSets` is on. The last set in `ApplySource` commits them.
		void ApplySourceLevel(IKsPropertySet* propertySet, void* gameSound, std::uint32_t soundFlags) {
			const auto buffer = static_cast<std::uintptr_t>(Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer));
			const float level = GetSourceLevel(soundFlags, &Field<char>(gameSound, engine::kSound_FilePath));

			if (level == 0.0f && !g_leveledBuffers.contains(buffer)) {
				return;
			}

			// OpenAL Soft lets both levels go above 0 dB, up to +10 dB.
			// Reference at "al/source.cpp", "eax_create_direct_filter_param".
			const LONG millibels = std::clamp(static_cast<LONG>(std::lround(level * 100.0f)), eax::kMinLevel,
				eax::kMaxSourceLevel);

			eax::SetProperty(propertySet, eax::kSource, Deferred(eax::kSource_Direct), &millibels, sizeof(millibels), "direct level");
			eax::SetProperty(propertySet, eax::kSource, Deferred(eax::kSource_Room), &millibels, sizeof(millibels), "room level");

			if (level == 0.0f) {
				g_leveledBuffers.erase(buffer);

				return;
			}

			// Released buffers are never removed, so the set is cleared when it grows too large.
			if (g_leveledBuffers.size() >= kMaxLeveledBuffers) {
				g_leveledBuffers.clear();
			}

			g_leveledBuffers.insert(buffer);
		}

		// Sets the primary FX slot volume to the configured wet level or to silent while bypassed. 
		// Bails if volume is unchanged.
		void ApplyVolume(IKsPropertySet* propertySet) {
			LONG volume = eax::DecibelsToMillibels(config::Get().spatialization.wetLevel);

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

		// Sets FX slot 0 to the listener environment and the wet level. Sets the impact tail in FX slot 2 to match.
		// Returns true if reverb parameters of slot 0 changed.
		bool ApplySlot(IKsPropertySet* propertySet) {
			const std::uint32_t environment = ResolveEnvironment();
			const bool environmentChanged = ApplyEnvironment(propertySet, environment);
			ApplyVolume(propertySet);
			ApplyTailSlot(propertySet, environment);

			return environmentChanged;
		}

		// Sets the active FX slots of a sound and its send levels. Slot 0 gets the route's send level.
		// Slot 2 is active only with a tail send level (gunfire and explosions in exteriors).
		// A sound with its send set to off (muted) and no tail does not get a slot.
		//
		// `keepSlot`: the sound can be occluded. OpenAL Soft applies source occlusion to the direct path only while
		// slot 0 is active, so such a sound keeps slot 0 with a silent send instead of null slots.
		void ApplySource(IKsPropertySet* propertySet, const Route& route, bool keepSlot, std::optional<float> tailSendLevel) {
			eax::ActiveFXSlots slots{};

			if (route.sendLevel <= config::kSendOff && !keepSlot && !tailSendLevel) {
				eax::SetProperty(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots), "active slots (none)");

				return;
			}

			slots.slots[0] = eax::kFXSlot0;

			if (tailSendLevel) {
				slots.slots[1] = eax::kFXSlot2;
			}

			const ULONG propertyId = Deferred(eax::kSource_ActiveFXSlotID);

			if (!eax::SetProperty(propertySet, eax::kSource, propertyId, &slots, sizeof(slots), "active slots")) {
				return;
			}

			const eax::SourceSendProperties send{eax::kFXSlot0, eax::DecibelsToMillibels(route.sendLevel), 0};  // Off: -10000 mB

			if (!tailSendLevel) {
				eax::SetProperty(propertySet, eax::kSource, eax::kSource_SendParameters, &send, sizeof(send), "send level");

				return;
			}

			const eax::SourceSendProperties sends[2] = {
				send,
				{eax::kFXSlot2, eax::DecibelsToMillibels(*tailSendLevel), 0},
			};

			eax::SetProperty(propertySet, eax::kSource, eax::kSource_SendParameters, sends, sizeof(sends), "send levels (tail)");
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
		if (!config::Get().spatialization.enabled) {
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
		const Route route = Classify(soundFlags, engine::IsHolotapeSound(gameSound));
		const bool keepSlot = config::Get().occlusion.enabled && occlusion::HasPosition(gameSound);
		ApplySourceLevel(propertySet, gameSound, soundFlags);
		ApplySource(propertySet, route, keepSlot, GetTailSendLevel(gameSound));

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
