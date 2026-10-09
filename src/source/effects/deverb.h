#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::effects {

	// A set of parameters for removing the baked-in reverb tail of a single gunshot.
	struct DeverbParameters {
		double keepFraction;  // Of the sound length, (0, 1]
		double fadeThreshold;  // dB below the peak. Fade starts once the level stays below it.
		double maxFadeDelay;  // Seconds after the peak
		double decayRate;  // dB per second
		double repeatLevel;  // dB below the peak. A later peak at this level counts as another shot.
	};

	enum class DeverbResult {
		Applied,
		Silent,
		LatePeak,  // Peak is in the part that would be cut
		RepeatedPeak,  // Sound has more than one shot
	};

	// The outcome of a deverb on a sound, with positions in frames.
	//
	// Positions after the result are only set as far as the deverb got.
	struct DeverbReport {
		DeverbResult result = DeverbResult::Silent;

		double peakLevel = 0.0;  // dBFS
		std::size_t peakFrame = 0;
		std::size_t fadeStartFrame = 0;
		std::size_t endFrame = 0;

		std::size_t repeatFrame = 0;
		double repeatLevel = 0.0;  // dB relative to the peak
	};

	DeverbReport ApplyDeverb(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const DeverbParameters& parameters);

}
