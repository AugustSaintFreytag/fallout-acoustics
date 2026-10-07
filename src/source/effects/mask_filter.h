#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::effects {
	struct MaskFilterPreset {
		double lowCutHz;  // 0 = off
		double resonanceHz;  // Air space inside the cover
		double resonanceGainDb;  // 0 = off
		double presenceHz;  // Speaker cone, intelligibility
		double presenceGainDb;  // 0 = off
		double highCutHz;
		bool steepHighCut;  // 4th order with a small peak before the cutoff, else 2nd order
		double driveDb;  // Speaker distortion, 0 = off
		double gainDb;  // Loudness relative to the unfiltered voice
	};

	// Filters interleaved 16-bit PCM in place: low cut, resonance, presence, high cut, distortion.
	// The result has the loudness of the input plus `preset.gainDb`, limited to avoid clipping.
	// Returns the gain applied after the filters, in dB.
	double ApplyMaskFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const MaskFilterPreset& preset);
}
