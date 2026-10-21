#include "occlusion/occlusion.h"

#include "config/settings.h"
#include "debug/beeper.h"
#include "engine/objects.h"
#include "engine/scene.h"
#include "occlusion/description.h"
#include "occlusion/probe.h"
#include "occlusion/registry.h"
#include "occlusion/statistics.h"
#include "utils/log.h"
#include "utils/timing.h"
#include "utils/vector3.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sea::occlusion {

	namespace {

		// Sounds this close to the listener or the player are never occluded.
		// In 3rd person the camera is the listener and the player's own sounds play at the player.
		constexpr float kMinDistance = 64.0f;

		struct ProbeState {
			std::int64_t lastProbeTime = 0;  // 0 = never probed
			std::int32_t targetMb = 0;
		};

		// Thread: Main (Read, Write)
		// Thread: Audio (Read)
		std::atomic<bool> g_bypass{false};

		// Thread: Main
		std::unordered_map<std::uint32_t, ProbeState> g_probeStates;

		// Returns occlusion target in mB for a sum of layer weights. A weight of 1 counts `fWallLevel`.
		// Limited to `fMaxOcclusion`.
		std::int32_t TargetForWeight(float weight) {
			const config::OcclusionSettings& settings = config::Get().occlusion;
			const float levelDb = std::min(settings.wallLevel * weight, settings.maxOcclusion);

			return -static_cast<std::int32_t>(std::lround(levelDb * 100.0f));
		}

		void LogTarget(const LiveSound& sound, std::int32_t previousMb, std::int32_t targetMb, const ProbeResult& result,
			float distance) {
			if (!config::Get().debug.logOcclusion || previousMb == targetMb) {
				return;
			}

			SEA_LOG("[Occlusion] Sound #%u, emitter: %s, occlusion: %s, casts: %d, distance: %.0fu (path: \"%.120s\")",
				sound.soundId, DescriptionForEmitter(result.emitter).c_str(), DescriptionForOcclusion(result).c_str(),
				result.casts, distance, GetPath(sound.soundId).c_str());
		}

		// Checks if a sound needs rays. Sounds too close to listener or player and sounds out of range get target 0.
		bool IsInProbeRange(const LiveSound& sound, const Vector3& listener, const Vector3& player) {
			const float distance = Distance(listener, sound.position);

			const bool isTooClose = distance < kMinDistance || Distance(player, sound.position) < kMinDistance;
			// Engine might play sounds past their max attenuation distance but can't be heard.
			const bool isInaudible = sound.maxDistance > 0.0f && distance > sound.maxDistance;
			const bool isTooFar = distance > config::Get().occlusion.maxDistance || isInaudible;

			return !isTooClose && !isTooFar;
		}

		// Removes the probe states of sounds that are gone.
		void PruneStates(const std::vector<LiveSound>& sounds) {
			std::unordered_set<std::uint32_t> liveIds;
			liveIds.reserve(sounds.size());

			for (const LiveSound& sound : sounds) {
				liveIds.insert(sound.soundId);
			}

			for (auto state = g_probeStates.begin(); state != g_probeStates.end();) {
				if (liveIds.count(state->first) == 0) {
					state = g_probeStates.erase(state);
				} else {
					++state;
				}
			}
		}

	}

	// Probes the live 3D sounds under the cast budget and publishes their occlusion targets.
	//
	// Thread: Main (each frame)
	void UpdateOcclusion() {
		const config::OcclusionSettings& settings = config::Get().occlusion;

		if (!settings.enabled) {
			return;
		}

		Vector3 listener;
		Vector3 forward;

		if (!engine::GetCameraTransform(listener, forward)) {
			return;
		}

		const Vector3 player = engine::GetPosition(engine::GetPlayer());
		std::vector<LiveSound> sounds = CollectSounds();
		
		PruneStates(sounds);

		// Never probed first, then the oldest probes.
		std::sort(sounds.begin(), sounds.end(), [](const LiveSound& left, const LiveSound& right) {
			return g_probeStates[left.soundId].lastProbeTime < g_probeStates[right.soundId].lastProbeTime;
		});

		const std::int64_t timeNow = timing::Now();
		const double refreshTimeInterval = settings.refreshInterval * 1000.0;

		int rayBudgetRemaining = settings.rayBudget;
		
		FrameStatistics frame;
		frame.numberOfLiveSounds = static_cast<int>(sounds.size());

		for (const LiveSound& sound : sounds) {
			if (IsInProbeRange(sound, listener, player)) {
				frame.numberOfSoundsInRange += 1;
			}
		}

		for (const LiveSound& sound : sounds) {
			ProbeState& state = g_probeStates[sound.soundId];

			if (state.lastProbeTime != 0 && timing::TicksToMilliseconds(static_cast<double>(timeNow - state.lastProbeTime)) < refreshTimeInterval) {
				// Sorted by probe time, later sounds are more recent.
				break;
			}

			const float distance = Distance(listener, sound.position);
			ProbeResult result;

			if (IsInProbeRange(sound, listener, player)) {
				if (rayBudgetRemaining < kMaxCastsPerProbe) {
					frame.isBudgetDepleted = true;
					break;
				}

				result = ProbeOcclusion(listener, sound.position);
				rayBudgetRemaining -= result.casts;

				frame.numberOfProbes += 1;
				frame.numberOfCasts += result.casts;
			}

			const std::int32_t targetMb = TargetForWeight(result.weight);
			const std::int32_t previousMb = state.targetMb;

			state.lastProbeTime = timeNow;
			state.targetMb = targetMb;
			
			SetTarget(sound.soundId, targetMb);
			LogTarget(sound, previousMb, targetMb, result, distance);

			debug::UpdateBeeper(sound.position, targetMb);
		}

		RecordFrame(frame);
	}

	// Toggles the occlusion bypass. Every sound ramps to 0 while bypassed.
	// Returns true if occlusion is active after the toggle.
	//
	// Thread: Main
	bool ToggleBypass() {
		const bool bypass = !g_bypass.load();
		g_bypass.store(bypass);

		if (bypass) {
			SEA_LOG("[Occlusion] Bypass is on.");
		} else {
			SEA_LOG("[Occlusion] Bypass is off.");
		}

		return !bypass;
	}

	// Thread: Any
	bool IsBypassed() {
		return g_bypass.load(std::memory_order_relaxed);
	}

}
