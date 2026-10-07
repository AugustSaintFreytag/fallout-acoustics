#include "reverb.h"

#include "dsound_util.h"
#include "eax.h"
#include "game.h"
#include "log.h"
#include "memory.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace sea::reverb {
	using mem::Field;

	namespace {
		constexpr eax::ReverbProperties kPresets[] = {
#include "reverb_presets.inc"
		};

		constexpr std::uint32_t kEnvironmentCount = static_cast<std::uint32_t>(std::size(kPresets));  // ANAM 1..30
		static_assert(kEnvironmentCount == 30);

		Config g_config;

		// Main thread writes, audio thread only reads.
		std::atomic<std::uint32_t> g_desiredEnvironment{0};  // ANAM, 0 = unknown
		std::atomic<bool> g_bypass{false};

		// Audio thread only.
		enum class EaxState { Unknown, Available, Unavailable };
		EaxState g_eaxState = EaxState::Unknown;
		std::uint32_t g_appliedEnvironment = 0;
		LONG g_appliedVolume = 1;  // Invalid level, first apply always runs.
		int g_failuresLogged = 0;
		std::uint32_t g_readbacksDone = 0;

		// Main thread only.
		std::uint32_t g_lastLoggedEnvironment = 0;
		void* g_lastLoggedSpace = nullptr;

		LONG DbToMb(float decibels) {
			const LONG millibels = static_cast<LONG>(std::lround(decibels * 100.0f));
			return std::clamp(millibels, eax::kMinLevel, 0L);
		}

		bool Set(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, const void* data, ULONG size,
			const char* description) {
			const HRESULT result = propertySet->Set(propertySetId, propertyId, nullptr, 0, const_cast<void*>(data), size);

			if (FAILED(result) && g_failuresLogged < 10) {
				++g_failuresLogged;
				SEA_LOG("[Reverb] Could not set EAX property: %s (HRESULT %08lX).", description, static_cast<unsigned long>(result));
			}

			return SUCCEEDED(result);
		}

		bool Get(IKsPropertySet* propertySet, const GUID& propertySetId, ULONG propertyId, void* data, ULONG size) {
			ULONG bytesReturned = 0;
			const HRESULT result = propertySet->Get(propertySetId, propertyId, nullptr, 0, data, size, &bytesReturned);

			if (FAILED(result)) {
				SEA_LOG("[Readback] Could not get EAX property %08lX (HRESULT %08lX).", propertyId, static_cast<unsigned long>(result));
			}

			return SUCCEEDED(result);
		}

		const char* SlotName(const GUID& slotId) {
			if (slotId == eax::kNull) {
				return "null";
			}

			if (slotId == eax::kFXSlot0) {
				return "FXSlot0";
			}

			return "other";
		}

		void ReadBackSlot(IKsPropertySet* propertySet) {
			eax::ReverbProperties reverb{};
			LONG volume = 0;

			if (!Get(propertySet, eax::kFXSlot0, eax::kReverb_AllParameters, &reverb, sizeof(reverb))) {
				return;
			}

			if (!Get(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume))) {
				return;
			}

			SEA_LOG("[Readback] Slot 0: Volume %ld mB, Env %u, Size %.1f, Room %ld, RoomHF %ld, Decay %.2fs, Reflections %ld, Reverb %ld",
				volume, reverb.environment, reverb.environmentSize, reverb.room, reverb.roomHF, reverb.decayTime,
				reverb.reflections, reverb.reverb);
		}

		void ReadBackSource(IKsPropertySet* propertySet, const char* label) {
			eax::ActiveFXSlots slots{};
			eax::SourceSendProperties sends[4]{};  // OpenAL Soft writes EAX_MAX_FXSLOTS (4) entries

			if (!Get(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots))) {
				return;
			}

			if (!Get(propertySet, eax::kSource, eax::kSource_SendParameters, sends, sizeof(sends))) {
				return;
			}

			SEA_LOG("[Readback] Source (%s): Active {%s, %s}, Send[0] %s %ld mB", label, SlotName(slots.slots[0]),
				SlotName(slots.slots[1]), SlotName(sends[0].receivingFXSlotID), sends[0].send);
		}

		void ApplyEnvironment(IKsPropertySet* propertySet) {
			std::uint32_t environment = g_desiredEnvironment.load(std::memory_order_relaxed);

			if (g_config.forceEnvironment) {
				environment = g_config.forceEnvironment;
			}

			if (environment == 0 || environment == g_appliedEnvironment) {
				return;
			}

			eax::ReverbProperties preset = kPresets[environment - 1];
			const LONG roomBoost = static_cast<LONG>(std::lround(g_config.roomBoostDb * 100.0f));
			preset.room = std::clamp(preset.room + roomBoost, eax::kMinLevel, 0L);

			if (!Set(propertySet, eax::kFXSlot0, eax::kReverb_AllParameters, &preset, sizeof(preset), "reverb parameters")) {
				return;
			}

			g_appliedEnvironment = environment;

			const char* forcedSuffix = "";

			if (g_config.forceEnvironment) {
				forcedSuffix = " [Forced]";
			}

			SEA_LOG("[Reverb] Slot 0 <- %s%s (Decay %.2fs, Room %ld mB)", game::EnvironmentTypeName(environment),
				forcedSuffix, preset.decayTime, preset.room);

			if (g_config.readbackCount) {
				ReadBackSlot(propertySet);
			}
		}

		void ApplyVolume(IKsPropertySet* propertySet) {
			LONG volume = DbToMb(g_config.wetLevelDb);

			if (g_bypass.load(std::memory_order_relaxed)) {
				volume = eax::kMinLevel;
			}

			if (volume == g_appliedVolume) {
				return;
			}

			if (!Set(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume), "slot volume")) {
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
		void ApplySlot(IKsPropertySet* propertySet) {
			ApplyEnvironment(propertySet);
			ApplyVolume(propertySet);
		}

		struct Route {
			const char* label;
			float sendDb;
		};

		Route Classify(std::uint32_t flags) {
			using namespace game;

			if (flags & (kSound_SystemSound | kSound_Music | kSound_Radio)) {
				return {"excluded", kSendOff};
			}

			const bool is2D = !(flags & kSound_3D);

			if (flags & kSound_Voice) {
				if (is2D) {
					return {"voice2D", g_config.sendVoice2D};
				}

				return {"voice3D", g_config.sendVoice3D};
			}

			if (flags & (kSound_2DGunfire | kSound_Battle)) {
				return {"weapons", g_config.sendWeapons};
			}

			if (flags & kSound_Footsteps) {
				return {"footsteps", g_config.sendFootsteps};
			}

			if (flags & kSound_Region) {
				return {"region", g_config.sendRegion};
			}

			if (flags & kSound_Loop) {
				if (is2D) {
					return {"loop2D", g_config.sendLoops2D};
				}

				return {"loop3D", g_config.sendLoops3D};
			}

			if (is2D) {
				return {"default2D", g_config.sendDefault2D};
			}

			return {"default3D", g_config.sendDefault3D};
		}

		void ApplySource(IKsPropertySet* propertySet, const Route& route) {
			eax::ActiveFXSlots slots{};

			if (route.sendDb <= kSendOff) {
				Set(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots), "active slots (none)");

				return;
			}

			slots.slots[0] = eax::kFXSlot0;

			if (!Set(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots), "active slots")) {
				return;
			}

			const eax::SourceSendProperties send{eax::kFXSlot0, DbToMb(route.sendDb), 0};
			Set(propertySet, eax::kSource, eax::kSource_SendParameters, &send, sizeof(send), "send level");
		}

		// EAX 4 slots 0 and 1 are locked legacy slots (OpenAL Soft: eax4_fx_slot_ensure_unlocked).
		// Slot 0 always holds a reverb, and LOADEFFECT on it fails. Can do a volume write to probe.
		bool ProbeEax(IKsPropertySet* propertySet) {
			const LONG volume = eax::kMinLevel;

			if (!Set(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume), "probe slot 0 volume")) {
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

	void Configure(const Config& config) {
		g_config = config;
	}

	const Config& GetConfig() {
		return g_config;
	}

	void UpdateListenerEnvironment() {
		void* player = game::GetPlayer();
		void* cell = game::GetParentCell(player);

		if (!cell) {
			return;
		}

		void* space = mem::Global<void*>(game::kCurrentAcousticSpace);
		const char* source = "current ASPC";

		if (!space) {
			space = game::GetCellAcousticSpace(cell);
			source = "cell ASPC";
		}

		std::uint32_t environment = 0;

		if (space) {
			environment = Field<std::uint32_t>(space, game::kAspc_EnvironmentType);
		}

		if (environment == 0 || environment > kEnvironmentCount) {
			const bool isInterior = Field<std::uint8_t>(cell, game::kCell_Flags) & 1;

			if (isInterior) {
				environment = g_config.interiorFallback;
				source = "interior fallback";
			} else {
				environment = g_config.exteriorFallback;
				source = "exterior fallback";
			}
		}

		g_desiredEnvironment.store(environment, std::memory_order_relaxed);

		if (environment == g_lastLoggedEnvironment && space == g_lastLoggedSpace) {
			return;
		}

		g_lastLoggedEnvironment = environment;
		g_lastLoggedSpace = space;

		if (space) {
			SEA_LOG("[Reverb] Listener Environment: %s (%s %s)", game::EnvironmentTypeName(environment), source,
				game::GetEditorID(space));
		} else {
			SEA_LOG("[Reverb] Listener Environment: %s (%s)", game::EnvironmentTypeName(environment), source);
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

	const char* OnSoundPlay(void* gameSound) {
		if (!g_config.enabled) {
			return "disabled";
		}

		if (g_eaxState == EaxState::Unavailable) {
			return "no-eax";
		}

		const std::uint32_t buffer = Field<std::uint32_t>(gameSound, game::kWin32Sound_Buffer);

		if (!ds::IsDSoundObject(buffer)) {
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

		ApplySlot(propertySet);

		const Route route = Classify(Field<std::uint32_t>(gameSound, game::kSound_TypeFlags));
		ApplySource(propertySet, route);

		if (g_readbacksDone < g_config.readbackCount && route.sendDb > kSendOff) {
			++g_readbacksDone;
			ReadBackSource(propertySet, route.label);
		}

		propertySet->Release();

		return route.label;
	}
}
