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
#include "utils/log.h"
#include "utils/memory.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace sea::reverb {
	using mem::Field;

	namespace {
		// Main thread writes, audio thread only reads.
		std::atomic<bool> g_bypass{false};

		// Audio thread only.
		enum class EaxState { Unknown, Available, Unavailable };
		EaxState g_eaxState = EaxState::Unknown;
		std::uint32_t g_appliedEnvironment = 0;
		LONG g_appliedVolume = 1;  // Invalid level, first apply always runs.

		// Adds the deferred flag if `[Debug] bDeferEaxSets` is on.
		// The last set in `ApplySource` is immediate and commits all deferred values.
		// If a set fails before it, the next sound commits the values.
		ULONG Deferred(ULONG propertyId) {
			if (!config::Get().debug.deferEaxSets) {
				return propertyId;
			}

			return propertyId | eax::kDeferred;
		}

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
			const LONG roomBoost = static_cast<LONG>(std::lround(settings.reverb.roomBoostDb * 100.0f));
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

		void ApplyVolume(IKsPropertySet* propertySet) {
			LONG volume = eax::DecibelsToMillibels(config::Get().reverb.wetLevelDb);

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

		// Set FX slot 0 to the desired env and fx wet level, if values have changed.
		// Returns true if the reverb parameters changed.
		bool ApplySlot(IKsPropertySet* propertySet) {
			const bool environmentChanged = ApplyEnvironment(propertySet);
			ApplyVolume(propertySet);

			return environmentChanged;
		}

		void ApplySource(IKsPropertySet* propertySet, const Route& route) {
			eax::ActiveFXSlots slots{};

			if (route.sendDb <= config::kSendOff) {
				eax::SetProperty(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots), "active slots (none)");

				return;
			}

			slots.slots[0] = eax::kFXSlot0;

			const ULONG propertyId = Deferred(eax::kSource_ActiveFXSlotID);

			if (!eax::SetProperty(propertySet, eax::kSource, propertyId, &slots, sizeof(slots), "active slots")) {
				return;
			}

			const eax::SourceSendProperties send{eax::kFXSlot0, eax::DecibelsToMillibels(route.sendDb), 0};
			eax::SetProperty(propertySet, eax::kSource, eax::kSource_SendParameters, &send, sizeof(send), "send level");
		}

		// EAX 4 slots 0 and 1 are locked legacy slots (OpenAL Soft: eax4_fx_slot_ensure_unlocked).
		// Slot 0 always holds a reverb, and LOADEFFECT on it fails. Can do a volume write to probe.
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

	bool IsBypassed() {
		return g_bypass.load(std::memory_order_relaxed);
	}

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

		const Route route = Classify(Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags));
		ApplySource(propertySet, route);

		// Read back after ApplySource, so that deferred values are committed.
		if (environmentChanged) {
			debug::ReadBackSlot(propertySet);
		}

		if (route.sendDb > config::kSendOff) {
			debug::ReadBackSource(propertySet, route.label);
		}

		propertySet->Release();

		return route.label;
	}
}
