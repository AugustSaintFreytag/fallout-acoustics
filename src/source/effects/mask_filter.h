#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::effects {

	// A preset of sound processing parameters for worn masks.
	struct MaskFilterPreset {
		double lowCutFrequency;  // 0 = off
		double highCutFrequency;

		double resonanceFrequency;  // Air space inside the cover
		double resonanceGainLevel;  // 0 = off
		
		double presenceFrequency;  // Speaker cone, intelligibility
		double presenceGainLevel;  // 0 = off
		
		bool steepHighCut;  // 4th order with a small peak before the cutoff, else 2nd order
		
		double driveLevel;  // Speaker distortion, 0 = off
		double gainLevel;  // Loudness relative to the unfiltered voice
	};

	double ApplyMaskFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const MaskFilterPreset& preset);

}
