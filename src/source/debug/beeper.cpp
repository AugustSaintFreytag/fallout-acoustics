#include "debug/beeper.h"

#include "config/settings.h"
#include "engine/objects.h"
#include "utils/log.h"
#include "utils/timing.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

namespace sea::debug {

	namespace {

		constexpr int kSampleRate = 44100;
		constexpr int kBlockSamples = 441;  // 10 ms
		constexpr int kBlockCount = 4;

		// Gain change per sample. Fades in or out in about 5 ms.
		constexpr float kGainStep = 1.0f / (kSampleRate * 0.005f);

		constexpr float kLevel = 0.1f;  // -20 dBFS
		constexpr float kOpenFrequency = 880.0f;  // Hz at 0 mB
		constexpr float kMinFrequency = 110.0f;  // Hz

		// Tone stops when the selected sound got no new target for this long.
		constexpr double kTargetTimeoutMs = 500.0;

		constexpr float kTwoPi = 6.28318530718f;

		// Sounds this close to the origin of the last console selection are assumed to be the emitter.
		constexpr float kSelectionRadius = 8.0f;  // Game units

		// Thread: Main (Write)
		// Thread: Beeper (Read)
		std::atomic<std::int32_t> g_targetMb{0};
		std::atomic<std::int64_t> g_targetTime{0};

		std::once_flag g_threadStarted;

		// Origin of the last console selection.
		//
		// Thread: Main
		Vector3 g_selectionPosition;
		
		bool g_hasSelection = false;

		// Returns the tone frequency for an occlusion target. 
		// Each layer of occlusion sounds one octave lower.
		float FrequencyForTarget(std::int32_t targetMb) {
			const float wallMb = config::Get().occlusion.wallLevel * 100.0f;

			if (wallMb <= 0.0f) {
				return kOpenFrequency;
			}

			const float frequency = kOpenFrequency * std::exp2(static_cast<float>(targetMb) / wallMb);

			if (frequency < kMinFrequency) {
				return kMinFrequency;
			}

			return frequency;
		}

		// Checks if the tone is on and the selected sound has a recent target.
		bool IsActive() {
			if (!config::Get().debug.occlusionBeep) {
				return false;
			}

			const std::int64_t targetTime = g_targetTime.load(std::memory_order_relaxed);

			if (targetTime == 0) {
				return false;
			}

			return timing::TicksToMilliseconds(static_cast<double>(timing::Now() - targetTime)) < kTargetTimeoutMs;
		}

		// Fills one block with a tone at the current target's frequency.
		void FillBlock(std::int16_t* samples, float& phase, float& gain) {
			float targetGain = 0.0f;

			if (IsActive()) {
				targetGain = 1.0f;
			}

			const float frequency = FrequencyForTarget(g_targetMb.load(std::memory_order_relaxed));
			const float phaseStep = kTwoPi * frequency / kSampleRate;

			for (int index = 0; index < kBlockSamples; ++index) {
				if (gain < targetGain) {
					gain = std::min(gain + kGainStep, targetGain);
				} else if (gain > targetGain) {
					gain = std::max(gain - kGainStep, targetGain);
				}

				samples[index] = static_cast<std::int16_t>(std::sin(phase) * gain * kLevel * 32767.0f);

				phase += phaseStep;

				if (phase >= kTwoPi) {
					phase -= kTwoPi;
				}
			}
		}

		// Opens a wave out device and keeps its blocks filled with the tone.
		//
		// Thread: Beeper
		void RunBeeperThread() {
			HANDLE blockDone = CreateEventA(nullptr, FALSE, FALSE, nullptr);

			WAVEFORMATEX format{};
			format.wFormatTag = WAVE_FORMAT_PCM;
			format.nChannels = 1;
			format.nSamplesPerSec = kSampleRate;
			format.wBitsPerSample = 16;
			format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
			format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

			HWAVEOUT device = nullptr;
			const MMRESULT result = waveOutOpen(&device, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(blockDone), 0,
				CALLBACK_EVENT);

			if (result != MMSYSERR_NOERROR) {
				SEA_LOG("[Beeper] Could not open wave out device (error %u).", result);
				return;
			}

			SEA_LOG("[Beeper] Started.");

			static std::int16_t blockSamples[kBlockCount][kBlockSamples];
			WAVEHDR headers[kBlockCount]{};
			float phase = 0.0f;
			float gain = 0.0f;

			for (int block = 0; block < kBlockCount; ++block) {
				headers[block].lpData = reinterpret_cast<LPSTR>(blockSamples[block]);
				headers[block].dwBufferLength = sizeof(blockSamples[block]);
				waveOutPrepareHeader(device, &headers[block], sizeof(WAVEHDR));

				FillBlock(blockSamples[block], phase, gain);
				waveOutWrite(device, &headers[block], sizeof(WAVEHDR));
			}

			while (true) {
				WaitForSingleObject(blockDone, INFINITE);

				for (int block = 0; block < kBlockCount; ++block) {
					if (!(headers[block].dwFlags & WHDR_DONE)) {
						continue;
					}

					headers[block].dwFlags &= ~WHDR_DONE;
					FillBlock(blockSamples[block], phase, gain);
					waveOutWrite(device, &headers[block], sizeof(WAVEHDR));
				}
			}
		}

	}

	// Keeps the origin of the current console selection, frame-wise.
	//
	// Thread: Main
	void PollBeeper() {
		if (!config::Get().debug.occlusionBeep) {
			return;
		}

		void* selection = engine::GetConsoleSelection();

		if (selection) {
			g_selectionPosition = engine::GetPosition(selection);
			g_hasSelection = true;
		}
	}

	// Sets the tone pitch from the occlusion target of a probed sound if it plays at the last console selection.
	//
	// Thread: Main
	void UpdateBeeper(const Vector3& soundPosition, std::int32_t targetMb) {
		if (!config::Get().debug.occlusionBeep || !g_hasSelection) {
			return;
		}

		if (Distance(g_selectionPosition, soundPosition) > kSelectionRadius) {
			return;
		}

		g_targetMb.store(targetMb, std::memory_order_relaxed);
		g_targetTime.store(timing::Now(), std::memory_order_relaxed);

		std::call_once(g_threadStarted, [] {
			std::thread(RunBeeperThread).detach();
		});
	}

}
