#include "reverb/threshold_shift.h"

#include "audio/directsound.h"
#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/sound_flags.h"
#include "engine/sound_paths.h"
#include "utils/memory.h"
#include "utils/threads.h"
#include "utils/timing.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

namespace sea::reverb {

	using mem::Field;

	namespace {

		// Time interval since last update after which an impact sound is considered stopped.
		constexpr double kImmureMaxUpdateGap = 50.0; // ms

		// Time interval since last update after which an impact sound is untracked.
		constexpr double kRemoveMaxUpdateGap = 1000.0; // ms

		// Update threshold to apply attenuated volume to a tracked impact sound.
		constexpr LONG kMinStepVolume = 10; // mB

		struct ShiftedSound {
			float sourceLevel = 0.0f;  // dB, from `[Sources]`
			LONG appliedMb = 0;
			std::int64_t lastUpdateTime = 0;
		};

		std::unordered_map<std::uint32_t, ShiftedSound> g_sounds;  // Sound ID -> state
		
		std::int64_t g_lastActiveTime = 0;
		
		float g_shift = 0.0f;  // dB, [fThresholdShiftAttenuation, 0]

		double MillisecondsBetween(std::int64_t earlier, std::int64_t later) {
			return timing::TicksToMilliseconds(static_cast<double>(later - earlier));
		}

		// Checks if the given sound should receive attenuation from a treshold shift.
		bool IsShiftedSound(void* gameSound) {
			const std::uint32_t soundFlags = Field<std::uint32_t>(gameSound, engine::kSound_TypeFlags);

			if (soundFlags & engine::kSound_3D) {
				return false;
			}

			const char* path = &Field<char>(gameSound, engine::kSound_FilePath);

			return engine::ClassifyGunfirePath(path) != engine::GunfireKind::None || engine::IsExplosionPath(path);
		}

		// Advances the threshold shift for all impact sounds, depending on time since last hit.
		void AdvanceShift(std::int64_t now) {
			const config::ImpactSettings& settings = config::Get().impacts;
			const float attenuation = settings.thresholdShiftAttenuation;
			const double timeElapsed = MillisecondsBetween(g_lastActiveTime, now); // ms

			g_lastActiveTime = now;

			if (timeElapsed <= kImmureMaxUpdateGap) {
				if (settings.thresholdShiftAttack <= 0.0f) {
					g_shift = attenuation;
					return;
				}

				const float step = attenuation * static_cast<float>(timeElapsed / 1000.0) / settings.thresholdShiftAttack;
				g_shift = std::max(g_shift + step, attenuation);

				return;
			}

			const double decayMs = timeElapsed - settings.thresholdShiftSustain * 1000.0;

			if (decayMs <= 0.0) {
				g_shift = std::max(g_shift, attenuation);
				return;
			}

			if (settings.thresholdShiftDecay <= 0.0f) {
				g_shift = 0.0f;
				return;
			}

			const float step = -attenuation * static_cast<float>(decayMs / 1000.0) / settings.thresholdShiftDecay;
			g_shift = std::clamp(g_shift + step, attenuation, 0.0f);
		}

		// Removes tracked shifted sounds without an update in a while.
		void RemoveExpiredSounds(std::int64_t now) {
			for (auto entry = g_sounds.begin(); entry != g_sounds.end();) {
				if (MillisecondsBetween(entry->second.lastUpdateTime, now) > kRemoveMaxUpdateGap) {
					entry = g_sounds.erase(entry);
				} else {
					++entry;
				}
			}
		}

		// Sets the direct and room level of a sound's buffer.
		bool SetLevel(void* gameSound, LONG millibels) {
			const std::uint32_t buffer = Field<std::uint32_t>(gameSound, engine::kWin32Sound_Buffer);

			if (!audio::IsDSoundObject(buffer)) {
				return false;
			}

			IKsPropertySet* propertySet = nullptr;
			const HRESULT result = reinterpret_cast<IUnknown*>(buffer)->QueryInterface(IID_IKsPropertySet,
				reinterpret_cast<void**>(&propertySet));

			if (FAILED(result) || !propertySet) {
				return false;
			}

			const bool directSet = eax::SetProperty(propertySet, eax::kSource, eax::kSource_Direct, &millibels, sizeof(millibels),
				"direct level (threshold shift)");
			const bool roomSet = eax::SetProperty(propertySet, eax::kSource, eax::kSource_Room, &millibels, sizeof(millibels),
				"room level (threshold shift)");

			propertySet->Release();

			return directSet && roomSet;
		}

	}

	// Starts tracking a shifted sound and moves the shift for its play.
	// Returns the given source level plus the current shift in dB for a shifted sound.
	//
	// Thread: Audio
	std::optional<float> OnThresholdShiftPlay(void* gameSound, float sourceLevel) {
		if (!config::Get().impacts.thresholdShift || !IsShiftedSound(gameSound)) {
			return std::nullopt;
		}

		const std::int64_t now = timing::Now();
		AdvanceShift(now);
		RemoveExpiredSounds(now);

		const float level = sourceLevel + g_shift;
		const std::uint32_t soundId = Field<std::uint32_t>(gameSound, engine::kSound_ID);
		g_sounds[soundId] = {sourceLevel, eax::SourceLevelToMillibels(level), now};

		return level;
	}

	// Moves the shift for a playing tracked impact sound and sets the sound's level.
	//
	// Thread: Audio
	void OnThresholdShiftUpdate(void* gameSound) {
		if (!config::Get().impacts.thresholdShift || !threads::IsAudioThread()) {
			return;
		}

		const auto entry = g_sounds.find(Field<std::uint32_t>(gameSound, engine::kSound_ID));

		if (entry == g_sounds.end()) {
			return;
		}

		const std::int64_t now = timing::Now();
		AdvanceShift(now);

		ShiftedSound& sound = entry->second;
		sound.lastUpdateTime = now;

		const LONG desiredMb = eax::SourceLevelToMillibels(sound.sourceLevel + g_shift);
		const float attenuation = config::Get().impacts.thresholdShiftAttenuation;
		const bool reachedEnd = (g_shift == 0.0f) || (g_shift == attenuation);

		if (desiredMb == sound.appliedMb) {
			return;
		}

		if (!reachedEnd && std::labs(desiredMb - sound.appliedMb) < kMinStepVolume) {
			return;
		}

		if (SetLevel(gameSound, desiredMb)) {
			sound.appliedMb = desiredMb;
		}
	}

}
