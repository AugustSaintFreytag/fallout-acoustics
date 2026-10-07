#pragma once

#include "config/settings.h"

#include <cstddef>
#include <cstdint>

namespace sea::effects {
	// Filters interleaved 16-bit PCM in place, for a voice heard through a mask or helmet:
	// low cut, mask resonance, steep high cut (4th order), speaker distortion.
	// The result has the loudness of the input plus `settings.gainDb`, limited to avoid clipping.
	// Returns the gain applied after the filters, in dB.
	double ApplyMaskFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const config::MaskSettings& settings);
}
