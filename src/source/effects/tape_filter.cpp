#include "effects/tape_filter.h"

#include "effects/biquad.h"
#include "effects/signal.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace sea::effects {

	namespace {

		constexpr double kButterworthQ = 0.7071;
		constexpr double kHeadBumpQ = 0.8;
		constexpr double kFlutterPhase = 1.3;  // Radians. Keeps wow and flutter peaks apart.

		// A sine oscillator by rotation. Cheaper than `std::sin` for each sample.
		struct Oscillator {
			double sine = 0.0;
			double cosine = 1.0;
			double stepSine = 0.0;
			double stepCosine = 1.0;

			Oscillator(double frequency, double sampleRate, double phase) {
				const double step = 2.0 * std::numbers::pi * frequency / sampleRate;

				sine = std::sin(phase);
				cosine = std::cos(phase);
				stepSine = std::sin(step);
				stepCosine = std::cos(step);
			}

			double Next() {
				const double value = sine;
				const double nextSine = sine * stepCosine + cosine * stepSine;
				cosine = cosine * stepCosine - sine * stepSine;
				sine = nextSine;

				return value;
			}
		};

		// Returns white noise in [-1, 1]. Xorshift with a fixed seed so the same buffer gets the same result.
		struct NoiseGenerator {
			std::uint32_t state;

			double Next() {
				state ^= state << 13;
				state ^= state >> 17;
				state ^= state << 5;

				return static_cast<double>(state) / 2147483647.5 - 1.0;
			}
		};

		// Adds hiss at the given level below the given voice level.
		void AddHiss(std::vector<double>& signal, double voiceLevel, double hissDb, unsigned channel) {
			if (hissDb <= -100.0) {
				return;
			}

			// Uniform noise in [-a, a] has an RMS of a / sqrt(3).
			const double amplitude = voiceLevel * DecibelsToGain(hissDb) * std::numbers::sqrt3;
			NoiseGenerator noise{0x9E3779B9u + channel};

			for (double& sample : signal) {
				sample += noise.Next() * amplitude;
			}
		}

		// Varies the playback speed with a slow (wow) and a fast (flutter) delay modulation.
		// The delay never goes below zero, so each output sample reads from the same or an earlier input position.
		std::vector<double> ApplyWowAndFlutter(const std::vector<double>& input, double sampleRate,
			const TapeFilterPreset& preset) {
			const double wowDepth = preset.wowDepth * sampleRate;
			const double flutterDepth = preset.flutterDepth * sampleRate;

			if (wowDepth <= 0.0 && flutterDepth <= 0.0) {
				return input;
			}

			Oscillator wow(preset.wowRate, sampleRate, 0.0);
			Oscillator flutter(preset.flutterRate, sampleRate, kFlutterPhase);
			std::vector<double> output(input.size());

			for (std::size_t frame = 0; frame < input.size(); ++frame) {
				const double delay = wowDepth * (1.0 + wow.Next()) + flutterDepth * (1.0 + flutter.Next());
				const double position = static_cast<double>(frame) - delay;

				if (position < 0.0) {
					output[frame] = 0.0;
					continue;
				}

				const auto index = static_cast<std::size_t>(position);
				const double fraction = position - static_cast<double>(index);
				const std::size_t nextIndex = std::min(index + 1, input.size() - 1);

				output[frame] = input[index] + (input[nextIndex] - input[index]) * fraction;
			}

			return output;
		}

		// An effects chain of playback filters. A default `Biquad` filter does not change the signal.
		struct PlaybackChain {
			Biquad lowCut;
			Biquad headBump;
			Biquad highCut1;
			Biquad highCut2;

			double Process(double sample) {
				sample = lowCut.Process(sample);
				sample = headBump.Process(sample);
				sample = highCut1.Process(sample);

				return highCut2.Process(sample);
			}
		};

		// Builds the playback filters of the given preset for the supplied sample rate.
		PlaybackChain MakePlaybackChain(double sampleRate, const TapeFilterPreset& preset) {
			// Keeps all frequencies well below Nyquist. Voice buffers can be 22 or 24 kHz.
			const double frequencyLimit = sampleRate * 0.45;
			const double highCut = std::min(preset.highCutFrequency, frequencyLimit);

			PlaybackChain chain;

			if (preset.lowCutFrequency > 0.0) {
				chain.lowCut = Biquad::HighPass(sampleRate, std::min(preset.lowCutFrequency, highCut * 0.5), kButterworthQ);
			}

			if (preset.headBumpGainLevel != 0.0) {
				const double headBump = std::min(preset.headBumpFrequency, frequencyLimit);
				chain.headBump = Biquad::Peaking(sampleRate, headBump, kHeadBumpQ, preset.headBumpGainLevel);
			}

			chain.highCut1 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);
			chain.highCut2 = Biquad::LowPass(sampleRate, highCut, kButterworthQ);

			return chain;
		}

	}

	// Filters interleaved 16-bit PCM like playback from an old tape. Each channel keeps its own loudness.
	// Order follows the tape: saturation and hiss on record, speed variation and head response on playback.
	// Returns the gain applied after the filters, in dB.
	double ApplyTapeFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const TapeFilterPreset& preset) {
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

			Saturate(signal, preset.driveLevel);
			AddHiss(signal, inputLevel, preset.hissLevel, channel);

			std::vector<double> played = ApplyWowAndFlutter(signal, sampleRate, preset);
			PlaybackChain chain = MakePlaybackChain(sampleRate, preset);

			for (double& sample : played) {
				sample = chain.Process(sample);
			}

			const double gain = LoudnessMatchGain(played, inputLevel, preset.gainLevel);

			if (gain <= 0.0) {
				continue;
			}

			for (std::size_t frame = 0; frame < frameCount; ++frame) {
				samples[frame * channelCount + channel] = ToPcm16(played[frame] * gain);
			}

			appliedGainDb = GainToDecibels(gain);
		}

		return appliedGainDb;
	}

}
