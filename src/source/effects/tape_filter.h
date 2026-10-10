#pragma once

#include <cstddef>
#include <cstdint>

namespace sea::effects {

	// A preset of sound processing parameters for playback from an old tape.
	struct TapeFilterPreset {
		double lowCutFrequency;  // 0 = off
		double highCutFrequency;  // 4th order

		double headBumpFrequency;  // Low boost of the playback head
		double headBumpGainLevel;  // 0 = off

		double driveLevel;  // Tape saturation, 0 = off
		double hissLevel;  // dB relative to the voice level, -100 = off

		// Speed variation as a modulated delay. Depth in seconds.
		double wowRate;
		double wowDepth;
		double flutterRate;
		double flutterDepth;

		double gainLevel;  // Loudness relative to the unfiltered sound
	};

	double ApplyTapeFilter(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const TapeFilterPreset& preset);

}
