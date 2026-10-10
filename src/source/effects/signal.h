#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sea::effects {

	constexpr double kPeakLimit = 0.97;  // Of full scale. Keeps headroom for rounding.

	inline double DecibelsToGain(double decibels) {
		return std::pow(10.0, decibels / 20.0);
	}

	inline double GainToDecibels(double gain) {
		return 20.0 * std::log10(gain);
	}

	inline double RootMeanSquare(const std::vector<double>& signal) {
		double sum = 0.0;

		for (const double sample : signal) {
			sum += sample * sample;
		}

		return std::sqrt(sum / static_cast<double>(signal.size()));
	}

	inline double Peak(const std::vector<double>& signal) {
		double peak = 0.0;

		for (const double sample : signal) {
			peak = std::max(peak, std::abs(sample));
		}

		return peak;
	}

	// Applies soft clipping (tanh) to the signal. Scaled to keep the peak level.
	inline void Saturate(std::vector<double>& signal, double driveDb) {
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

	// Returns the gain that brings the signal to the given input level plus the extra gain in dB.
	// Lowers the gain where the peak would clip. Returns 0 for a silent signal.
	inline double LoudnessMatchGain(const std::vector<double>& signal, double inputLevel, double gainDb) {
		const double outputLevel = RootMeanSquare(signal);

		if (outputLevel <= 0.0) {
			return 0.0;
		}

		double gain = inputLevel / outputLevel * DecibelsToGain(gainDb);
		const double peak = Peak(signal) * gain;

		if (peak > kPeakLimit) {
			gain *= kPeakLimit / peak;
		}

		return gain;
	}

	inline std::int16_t ToPcm16(double sample) {
		const long value = std::lround(sample * 32768.0);

		return static_cast<std::int16_t>(std::clamp(value, -32768L, 32767L));
	}

}
