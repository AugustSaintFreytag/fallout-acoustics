#include "effects/mask_filter.h"

#include "effects/biquad.h"
#include "effects/signal.h"

#include <algorithm>
#include <vector>

namespace sea::effects {

	namespace {

		constexpr double kButterworthQ = 0.7071;
		constexpr double kSpeakerHighCutQ = 0.9;  // Small peak before the cutoff, like a small speaker.
		constexpr double kResonanceQ = 1.2;
		constexpr double kPresenceQ = 1.5;

		// An effects chain of configured audio filters.
		//
		// A default `Biquad` filter doesn't change the signal.
		struct FilterChain {
			Biquad lowCut;
			Biquad resonance;
			Biquad presence;
			Biquad highCut1;
			Biquad highCut2;

			double Process(double sample) {
				sample = lowCut.Process(sample);
				sample = resonance.Process(sample);
				sample = presence.Process(sample);
				sample = highCut1.Process(sample);

				return highCut2.Process(sample);
			}
		};

		// Builds the filter stages of the given preset for the supplied sample rate.
		FilterChain MakeFilterChain(double sampleRate, const MaskFilterPreset& preset) {
			// Keeps all frequencies well below Nyquist. Voice buffers can be 22 or 24 kHz.
			const double frequencyLimit = sampleRate * 0.45;
			const double highCut = std::min(preset.highCutFrequency, frequencyLimit);

			FilterChain chain;

			if (preset.lowCutFrequency > 0.0) {
				chain.lowCut = Biquad::HighPass(sampleRate, std::min(preset.lowCutFrequency, highCut * 0.5), kButterworthQ);
			}

			if (preset.resonanceGainLevel != 0.0) {
				const double resonance = std::min(preset.resonanceFrequency, frequencyLimit);
				chain.resonance = Biquad::Peaking(sampleRate, resonance, kResonanceQ, preset.resonanceGainLevel);
			}

			if (preset.presenceGainLevel != 0.0) {
				const double presence = std::min(preset.presenceFrequency, frequencyLimit);
				chain.presence = Biquad::Peaking(sampleRate, presence, kPresenceQ, preset.presenceGainLevel);
			}

			if (preset.steepHighCut) {
				chain.highCut1 = Biquad::LowPass(sampleRate, highCut, kSpeakerHighCutQ);
				chain.highCut2 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);
			} else {
				chain.highCut1 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);
			}

			return chain;
		}

	}

	// Applies preset-based audio filter for worn masks.
	// Returns the gain applied after the filters, in dB.

	// Filter applied to interleaved 16-bit PCM. 
	// Applies low cut, resonance, presence, high cut, and distortion.
	// Compensates for loudness plus applied gain from selected preset.
	double ApplyMaskFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const MaskFilterPreset& preset) {
		double appliedGainDb = 0.0;

		if (frameCount == 0 || channelCount == 0 || sampleRate <= 0.0) {
			return appliedGainDb;
		}

		std::vector<double> signal(frameCount);

		// Each channel is filtered on its own and keeps its own loudness.
		for (unsigned channel = 0; channel < channelCount; ++channel) {
			for (std::size_t frame = 0; frame < frameCount; ++frame) {
				signal[frame] = samples[frame * channelCount + channel] / 32768.0;
			}

			const double inputLevel = RootMeanSquare(signal);

			if (inputLevel <= 0.0) {
				continue;
			}

			FilterChain chain = MakeFilterChain(sampleRate, preset);

			for (double& sample : signal) {
				sample = chain.Process(sample);
			}

			Saturate(signal, preset.driveLevel);

			const double gain = LoudnessMatchGain(signal, inputLevel, preset.gainLevel);

			if (gain <= 0.0) {
				continue;
			}

			for (std::size_t frame = 0; frame < frameCount; ++frame) {
				samples[frame * channelCount + channel] = ToPcm16(signal[frame] * gain);
			}

			appliedGainDb = GainToDecibels(gain);
		}

		return appliedGainDb;
	}

}
