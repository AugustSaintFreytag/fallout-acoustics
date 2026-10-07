#include "effects/mask_filter.h"

#include "effects/biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sea::effects {
	namespace {
		constexpr double kPeakLimit = 0.97;  // Of full scale. Keeps headroom for rounding.
		constexpr double kButterworthQ = 0.7071;
		constexpr double kSpeakerHighCutQ = 0.9;  // Small peak before the cutoff, like a small speaker.
		constexpr double kResonanceQ = 1.2;
		constexpr double kPresenceQ = 1.5;

		double DecibelsToGain(double decibels) {
			return std::pow(10.0, decibels / 20.0);
		}

		double GainToDecibels(double gain) {
			return 20.0 * std::log10(gain);
		}

		double RootMeanSquare(const std::vector<double>& signal) {
			double sum = 0.0;

			for (const double sample : signal) {
				sum += sample * sample;
			}

			return std::sqrt(sum / static_cast<double>(signal.size()));
		}

		double Peak(const std::vector<double>& signal) {
			double peak = 0.0;

			for (const double sample : signal) {
				peak = std::max(peak, std::abs(sample));
			}

			return peak;
		}

		// A default Biquad passes the signal unchanged, so stages that are off cost little.
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

		FilterChain MakeFilterChain(double sampleRate, const MaskFilterPreset& preset) {
			// Keeps all frequencies well below Nyquist. Voice buffers can be 22 or 24 kHz.
			const double frequencyLimit = sampleRate * 0.45;
			const double highCut = std::min(preset.highCutHz, frequencyLimit);

			FilterChain chain;

			if (preset.lowCutHz > 0.0) {
				chain.lowCut = Biquad::HighPass(sampleRate, std::min(preset.lowCutHz, highCut * 0.5), kButterworthQ);
			}

			if (preset.resonanceGainDb != 0.0) {
				const double resonance = std::min(preset.resonanceHz, frequencyLimit);
				chain.resonance = Biquad::Peaking(sampleRate, resonance, kResonanceQ, preset.resonanceGainDb);
			}

			if (preset.presenceGainDb != 0.0) {
				const double presence = std::min(preset.presenceHz, frequencyLimit);
				chain.presence = Biquad::Peaking(sampleRate, presence, kPresenceQ, preset.presenceGainDb);
			}

			if (preset.steepHighCut) {
				chain.highCut1 = Biquad::LowPass(sampleRate, highCut, kSpeakerHighCutQ);
				chain.highCut2 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);
			} else {
				chain.highCut1 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);
			}

			return chain;
		}

		// Soft clipping (tanh), scaled so that the peak level stays the same.
		void Saturate(std::vector<double>& signal, double driveDb) {
			if (driveDb <= 0.0) {
				return;
			}

			const double peak = Peak(signal);

			if (peak <= 0.0) {
				return;
			}

			const double drive = DecibelsToGain(driveDb);
			const double normalization = std::tanh(drive);

			for (double& sample : signal) {
				sample = std::tanh(drive * sample / peak) / normalization * peak;
			}
		}

		std::int16_t ToPcm16(double sample) {
			const long value = std::lround(sample * 32768.0);

			return static_cast<std::int16_t>(std::clamp(value, -32768L, 32767L));
		}
	}

	double ApplyMaskFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const MaskFilterPreset& preset) {
		double appliedGainDb = 0.0;

		if (frameCount == 0 || channelCount == 0 || sampleRate <= 0.0) {
			return appliedGainDb;
		}

		std::vector<double> signal(frameCount);

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

			Saturate(signal, preset.driveDb);

			const double outputLevel = RootMeanSquare(signal);

			if (outputLevel <= 0.0) {
				continue;
			}

			double gain = inputLevel / outputLevel * DecibelsToGain(preset.gainDb);
			const double peak = Peak(signal) * gain;

			if (peak > kPeakLimit) {
				gain *= kPeakLimit / peak;
			}

			for (std::size_t frame = 0; frame < frameCount; ++frame) {
				samples[frame * channelCount + channel] = ToPcm16(signal[frame] * gain);
			}

			appliedGainDb = GainToDecibels(gain);
		}

		return appliedGainDb;
	}
}
