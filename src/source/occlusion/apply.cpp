#include "occlusion/apply.h"

#include "audio/directsound.h"
#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "engine/addresses.h"
#include "occlusion/occlusion.h"
#include "occlusion/registry.h"
#include "utils/memory.h"
#include "utils/threads.h"
#include "utils/timing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

namespace sea::occlusion {

	using mem::Field;

	namespace {

		constexpr std::size_t kMaxTrackedSounds = 1024;

		// A set is made when the value moved at least this far and the last set is at least this old.
		// A set is also made when the value reached its target. This limits EAX commits during a ramp.
		constexpr LONG kMinStepMb = 50;
		constexpr double kMinSetIntervalMs = 8.0;

		struct SoundState {
			float currentMb = 0.0f;
			LONG appliedMb = 0;
			bool applied = false;
			bool hasTarget = false;  // False until the main thread's first result
			std::int64_t lastStepTime = 0;
			std::int64_t lastSetTime = 0;
		};

		// Thread: Audio
		std::unordered_map<std::uint32_t, SoundState> g_states;

		// Sets EAX occlusion on a sound's buffer with the ratios from settings. Returns true on success.
		bool SetOcclusion(void* sound, LONG occlusionMb) {
			const std::uint32_t buffer = Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer);

			if (!audio::IsDSoundObject(buffer)) {
				return false;
			}

			IKsPropertySet* propertySet = nullptr;
			const HRESULT result = reinterpret_cast<IUnknown*>(buffer)->QueryInterface(IID_IKsPropertySet,
				reinterpret_cast<void**>(&propertySet));

			if (FAILED(result) || !propertySet) {
				return false;
			}

			const config::OcclusionSettings& settings = config::Get().occlusion;
			const eax::OcclusionProperties occlusion{occlusionMb, settings.lfRatio, settings.roomRatio,
				eax::kDefaultOcclusionDirectRatio};

			const bool applied = eax::SetProperty(propertySet, eax::kSource, eax::kSource_OcclusionParameters, &occlusion,
				sizeof(occlusion), "occlusion");

			propertySet->Release();

			return applied;
		}

		// Returns the ramp rate in mB per second for the attack (more occlusion) or the release (less occlusion).
		float RampRate(bool attack) {
			const config::OcclusionSettings& settings = config::Get().occlusion;
			const float rangeMb = settings.maxOcclusion * 100.0f;
			float seconds = settings.releaseTime;

			if (attack) {
				seconds = settings.attackTime;
			}

			if (seconds <= 0.0f) {
				return rangeMb * 1000.0f;
			}

			return rangeMb / seconds;
		}

		// Reads the target of a sound. Target is 0 while bypassed and the last known target otherwise.
		// Returns false if the main thread has no target for it yet.
		bool ReadTarget(std::uint32_t soundId, LONG& targetMb) {
			std::int32_t registryTarget = 0;

			if (!GetTarget(soundId, registryTarget)) {
				return false;
			}

			targetMb = registryTarget;

			if (IsBypassed()) {
				targetMb = 0;
			}

			return true;
		}

		// Publishes a sound, moves its occlusion one step toward the target and sets it on the buffer if needed.
		// On play the value is always set. A reused buffer may still hold an old value.
		void Step(void* sound, bool isPlay) {
			const std::uint32_t soundId = Field<std::uint32_t>(sound, engine::kSound_ID);
			const Vector3 position = Field<Vector3>(sound, engine::kWin32Sound_EmitterPosition);
			const char* path = &Field<char>(sound, engine::kSound_FilePath);

			PublishSound(soundId, position, path);

			// States of ended sounds stay in the table. Clear it when full.
			if (g_states.size() >= kMaxTrackedSounds) {
				g_states.clear();
			}

			SoundState& state = g_states[soundId];
			const std::int64_t now = timing::Now();

			LONG targetMb = 0;
			const bool knownTarget = ReadTarget(soundId, targetMb);

			if (knownTarget && !state.hasTarget) {
				// The first result applies at once. A ramp from 0 would let the first part of the sound through.
				state.hasTarget = true;
				state.currentMb = static_cast<float>(targetMb);
			} else if (knownTarget && state.lastStepTime != 0) {
				const float seconds = static_cast<float>(timing::TicksToMilliseconds(static_cast<double>(now - state.lastStepTime)) / 1000.0);
				const bool attack = targetMb < state.currentMb;
				const float maxStep = RampRate(attack) * seconds;
				const float difference = static_cast<float>(targetMb) - state.currentMb;

				state.currentMb += std::clamp(difference, -maxStep, maxStep);
			}

			state.lastStepTime = now;

			const LONG desiredMb = std::lround(state.currentMb);

			if (!isPlay && state.applied && desiredMb == state.appliedMb) {
				return;
			}

			const bool reachedTarget = desiredMb == targetMb;
			const double msSinceSet = timing::TicksToMilliseconds(static_cast<double>(now - state.lastSetTime));
			const bool bigEnough = std::labs(desiredMb - state.appliedMb) >= kMinStepMb && msSinceSet >= kMinSetIntervalMs;

			if (!isPlay && state.applied && !reachedTarget && !bigEnough) {
				return;
			}

			if (SetOcclusion(sound, desiredMb)) {
				state.applied = true;
				state.appliedMb = desiredMb;
				state.lastSetTime = now;
			}
		}

	}

	// Checks if the given sound reference has a 3D position.
	// Exception: Script sounds may have a 3D flag on a 2D buffer but will not have a position.
	bool HasPosition(void* sound) {
		return Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer3D) != 0;
	}

	// Publishes a new 3D sound and sets its current occlusion. Sets 0 too. A reused buffer can hold an old value.
	// Called from `BSWin32GameSound::Play` after reverb routing and before the original `Play`.
	//
	// Thread: Audio
	void OnSoundPlay(void* sound) {
		if (!config::Get().occlusion.enabled || !HasPosition(sound)) {
			return;
		}

		Step(sound, true);
	}

	// Publishes the position of a 3D sound, ramps its occlusion toward the target and sets EAX when it changed enough.
	// Called from `BSWin32GameSound::Update` about every 2 ms per sound. Does nothing on other threads.
	//
	// Thread: Audio
	void OnSoundUpdate(void* sound) {
		if (!config::Get().occlusion.enabled || !threads::IsAudioThread() || !HasPosition(sound)) {
			return;
		}

		Step(sound, false);
	}

}
