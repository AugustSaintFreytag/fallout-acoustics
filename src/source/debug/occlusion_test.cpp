#include "debug/occlusion_test.h"

#include "audio/directsound.h"
#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/sound_flags.h"
#include "utils/log.h"
#include "utils/memory.h"
#include "utils/threads.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace sea::debug {

	using mem::Field;

	namespace {

		constexpr std::size_t kMaxTrackedSounds = 1024;
		constexpr int kReadbacksPerToggle = 4;
		constexpr int kMaxBufferChangeLogs = 20;

		struct AppliedState {
			std::uint32_t generation = 0;
			std::uint32_t buffer = 0;  // IDirectSoundBuffer that received the value
		};

		// Thread: Main (Read, Write)
		// Thread: Audio (Read)
		std::atomic<bool> g_active{true};
		std::atomic<std::uint32_t> g_generation{1};

		// Thread: Audio
		std::unordered_map<std::uint32_t, AppliedState> g_applied;  // Sound ID -> last applied state
		int g_bufferChangesLogged = 0;
		std::uint32_t g_readbackGeneration = 0;
		int g_readbacksLeft = 0;

		// Returns test value in mB or 0 while toggled off.
		LONG CurrentLevel() {
			if (!g_active.load(std::memory_order_relaxed)) {
				return 0;
			}

			return static_cast<LONG>(config::Get().debug.testOcclusion);
		}

		// Checks if a test value is set and live occlusion is off. Live occlusion replaces test values.
		bool IsEnabled() {
			const config::Settings& settings = config::Get();

			return settings.debug.testOcclusion != 0 && !settings.occlusion.enabled;
		}

		// Logs occlusion and active slots that OpenAL Soft holds for a sound.
		void ReadBack(IKsPropertySet* propertySet, std::uint32_t soundId) {
			eax::OcclusionProperties occlusion{};
			eax::ActiveFXSlots slots{};

			if (!eax::GetProperty(propertySet, eax::kSource, eax::kSource_OcclusionParameters, &occlusion, sizeof(occlusion))) {
				return;
			}

			if (!eax::GetProperty(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots))) {
				return;
			}

			// OpenAL Soft applies occlusion to the direct path only with slot 0 active.
			const char* slotNote = "slot 0 active";

			if (slots.slots[0] != eax::kFXSlot0 && slots.slots[1] != eax::kFXSlot0) {
				slotNote = "slot 0 NOT active, direct path not occluded";
			}

			SEA_LOG("[OcclusionTest] Readback ID=%u: Occlusion %ld mB, LF %.2f, Room %.2f, Direct %.2f (%s)", soundId,
				occlusion.occlusion, occlusion.occlusionLFRatio, occlusion.occlusionRoomRatio,
				occlusion.occlusionDirectRatio, slotNote);
		}

		// Sets current test value on a sound's buffer and remembers it. Reads back the first sets after each toggle.
		void Apply(void* sound, std::uint32_t generation) {
			const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);
			const std::uint32_t buffer = Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer);

			if (!audio::IsDSoundObject(buffer)) {
				return;
			}

			IKsPropertySet* propertySet = nullptr;
			const HRESULT result = reinterpret_cast<IUnknown*>(buffer)->QueryInterface(IID_IKsPropertySet,
				reinterpret_cast<void**>(&propertySet));

			if (FAILED(result) || !propertySet) {
				return;
			}

			const eax::OcclusionProperties occlusion{CurrentLevel(), eax::kDefaultOcclusionLFRatio,
				eax::kDefaultOcclusionRoomRatio, eax::kDefaultOcclusionDirectRatio};

			const bool applied = eax::SetProperty(propertySet, eax::kSource, eax::kSource_OcclusionParameters, &occlusion,
				sizeof(occlusion), "test occlusion");

			if (applied) {
				if (g_applied.size() >= kMaxTrackedSounds) {
					g_applied.clear();
				}

				g_applied[soundId] = {generation, buffer};

				if (g_readbackGeneration != generation) {
					g_readbackGeneration = generation;
					g_readbacksLeft = kReadbacksPerToggle;
				}

				if (g_readbacksLeft > 0) {
					--g_readbacksLeft;
					ReadBack(propertySet, soundId);
				}
			}

			propertySet->Release();
		}

		// Checks if the given sound reference has a 3D position.
		// Exception: Script sounds may have a 3D flag on a 2D buffer but will not have a position.
		bool Is3D(void* sound) {
			return Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer3D) != 0;
		}

	}

	// Toggles the test occlusion (`[Debug] iTestOcclusion`). Returns true if it is on after the toggle.
	//
	// Thread: Main
	bool ToggleTestOcclusion() {
		const bool active = !g_active.load();
		g_active.store(active);
		g_generation.fetch_add(1);

		if (active) {
			SEA_LOG("[OcclusionTest] On: %d mB on all 3D sounds.", config::Get().debug.testOcclusion);
		} else {
			SEA_LOG("[OcclusionTest] Off.");
		}

		return active;
	}

	// Sets test occlusion on a 3D sound. Does nothing while `[Occlusion] bEnabled` is on.
	// Called from `BSWin32GameSound::Play` after reverb routing.
	//
	// Thread: Audio
	void ApplyTestOcclusionOnPlay(void* sound) {
		if (!IsEnabled() || !Is3D(sound)) {
			return;
		}

		const std::uint32_t buffer = Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer);

		if (!audio::IsDSoundObject(buffer) && g_bufferChangesLogged < kMaxBufferChangeLogs) {
			++g_bufferChangesLogged;
			SEA_LOG("[OcclusionTest] ID=%u has no buffer at Play (%08X). Path=\"%.160s\"",
				Field<std::uint32_t>(sound, engine::kSound_ID), buffer, &Field<char>(sound, engine::kSound_FilePath));
		}

		Apply(sound, g_generation.load(std::memory_order_relaxed));
	}

	// Sets current test value on a playing 3D sound after a toggle or buffer change.
	// Called from `BSWin32GameSound::Update`. Does nothing on other threads.
	//
	// Thread: Audio
	void ApplyTestOcclusionOnUpdate(void* sound) {
		if (!IsEnabled() || !threads::IsAudioThread() || !Is3D(sound)) {
			return;
		}

		const std::uint32_t generation = g_generation.load(std::memory_order_relaxed);
		const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);
		const std::uint32_t buffer = Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer);
		const auto applied = g_applied.find(soundId);

		if (applied != g_applied.end() && applied->second.generation == generation && applied->second.buffer == buffer) {
			return;
		}

		// Engine can give a sound a new buffer after `Play`. Value on the old buffer is lost.
		if (applied != g_applied.end() && applied->second.buffer != buffer && g_bufferChangesLogged < kMaxBufferChangeLogs) {
			++g_bufferChangesLogged;
			SEA_LOG("[OcclusionTest] ID=%u has a new buffer after Play: %08X -> %08X. Path=\"%.160s\"", soundId,
				applied->second.buffer, buffer, &Field<char>(sound, engine::kSound_FilePath));
		}

		Apply(sound, generation);
	}

}
