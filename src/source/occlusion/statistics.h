#pragma once

namespace sea::occlusion {

	// Counts of one occlusion update.
	struct FrameStatistics {
		int numberOfLiveSounds = 0;
		int numberOfSoundsInRange = 0;
		int numberOfProbes = 0;
		int numberOfCasts = 0;
		bool isBudgetDepleted = false;
	};

	void RecordFrame(const FrameStatistics& frame);

}
