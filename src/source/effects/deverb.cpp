#include "effects/deverb.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace sea::effects {

	namespace {

		constexpr double kBlockDuration = 0.005;  // Seconds. Resolution of the level envelope.
		constexpr double kFadeHoldDuration = 0.02;  // Seconds below the fade threshold before the fade starts
		constexpr double kRepeatGap = 6.0;  // dB below the repeat level. Level must fall this far between two shots.
		constexpr double kDecayFloor = 96.0;  // dB. 16-bit noise floor. The sound ends where the decay gets here.
		constexpr double kEndFadeDuration = 0.005;  // Seconds. Linear fade before the end against clicks.
		constexpr double kFullScale = 32768.0;

		double DecibelsToGain(double decibels) {
			return std::pow(10.0, decibels / 20.0);
		}

		double GainToDecibels(double gain) {
			return 20.0 * std::log10(gain);
		}

		std::size_t SecondsToFrames(double seconds, double sampleRate) {
			return static_cast<std::size_t>(seconds * sampleRate);
		}

		// Returns the frame of the sample with the largest magnitude. The first one wins on a tie.
		std::size_t FindPeakFrame(const std::int16_t* samples, std::size_t frameCount, unsigned channelCount, int& peak) {
			const std::size_t sampleCount = frameCount * channelCount;
			std::size_t peakIndex = 0;
			peak = 0;

			for (std::size_t index = 0; index < sampleCount; ++index) {
				const int magnitude = std::abs(static_cast<int>(samples[index]));

				if (magnitude > peak) {
					peak = magnitude;
					peakIndex = index;
				}
			}

			return peakIndex / channelCount;
		}

		// Returns the largest sample magnitude in each block of frames.
		std::vector<int> MeasureBlockPeaks(const std::int16_t* samples, std::size_t frameCount, unsigned channelCount,
			std::size_t blockFrames) {
			std::vector<int> blockPeaks((frameCount + blockFrames - 1) / blockFrames, 0);

			for (std::size_t block = 0; block < blockPeaks.size(); ++block) {
				const std::size_t firstSample = block * blockFrames * channelCount;
				const std::size_t endSample = std::min(frameCount, (block + 1) * blockFrames) * channelCount;
				int blockPeak = 0;

				for (std::size_t index = firstSample; index < endSample; ++index) {
					blockPeak = std::max(blockPeak, std::abs(static_cast<int>(samples[index])));
				}

				blockPeaks[block] = blockPeak;
			}

			return blockPeaks;
		}

		// Returns the first block after the peak block from which the level stays below the given limit for the hold blocks.
		// Returns the block count if the level never gets there.
		std::size_t FindFadeStartBlock(const std::vector<int>& blockPeaks, std::size_t peakBlock, double limit,
			std::size_t holdBlocks) {
			for (std::size_t block = peakBlock + 1; block < blockPeaks.size(); ++block) {
				const std::size_t holdEnd = std::min(blockPeaks.size(), block + holdBlocks + 1);
				bool staysBelow = true;

				for (std::size_t heldBlock = block; heldBlock < holdEnd; ++heldBlock) {
					if (blockPeaks[heldBlock] >= limit) {
						staysBelow = false;
						break;
					}
				}

				if (staysBelow) {
					return block;
				}
			}

			return blockPeaks.size();
		}

		// Returns the first block from the given start block that rises above the repeat limit
		// after the level fell below the gap limit. Returns the block count if there is none.
		std::size_t FindRepeatBlock(const std::vector<int>& blockPeaks, std::size_t startBlock, double repeatLimit,
			double gapLimit) {
			bool hasGap = false;

			for (std::size_t block = startBlock; block < blockPeaks.size(); ++block) {
				if (blockPeaks[block] < gapLimit) {
					hasGap = true;
				} else if (hasGap && blockPeaks[block] >= repeatLimit) {
					return block;
				}
			}

			return blockPeaks.size();
		}

		// Applies an exponential decay from the fade start to the end frame and silences everything after it.
		// The last frames before the end also get a short linear fade.
		void ApplyDecay(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
			std::size_t fadeStartFrame, std::size_t endFrame, double decayRate) {
			const double gainStep = DecibelsToGain(-decayRate / sampleRate);
			const std::size_t endFadeFrames = std::min(SecondsToFrames(kEndFadeDuration, sampleRate), endFrame - fadeStartFrame);
			const std::size_t endFadeStartFrame = endFrame - endFadeFrames;
			double gain = 1.0;

			for (std::size_t frame = fadeStartFrame; frame < endFrame; ++frame) {
				double frameGain = gain;

				if (frame >= endFadeStartFrame) {
					frameGain *= static_cast<double>(endFrame - frame) / static_cast<double>(endFadeFrames);
				}

				std::int16_t* frameSamples = samples + frame * channelCount;

				for (unsigned channel = 0; channel < channelCount; ++channel) {
					frameSamples[channel] = static_cast<std::int16_t>(std::lround(frameSamples[channel] * frameGain));
				}

				gain *= gainStep;
			}

			std::fill(samples + endFrame * channelCount, samples + frameCount * channelCount, std::int16_t{0});
		}

	}

	// Removes the reverb tail of a single gunshot in place. The buffer length stays and the cut part is silenced.
	// The fade starts once the level after the peak stays below the fade threshold. It is cut at the max fade delay.
	// From there, the level falls by the decay rate until the noise floor or the keep fraction ends it.
	// Returns a report without changing the samples if the sound is silent, peaks late or has more than one shot.
	DeverbReport ApplyDeverb(std::int16_t* samples, std::size_t frameCount, unsigned channelCount, double sampleRate,
		const DeverbParameters& parameters) {
		DeverbReport report;

		if (frameCount == 0 || channelCount == 0) {
			return report;
		}

		int peak = 0;
		report.peakFrame = FindPeakFrame(samples, frameCount, channelCount, peak);

		if (peak == 0) {
			return report;
		}

		report.peakLevel = GainToDecibels(peak / kFullScale);

		const std::size_t keepFrames = std::max<std::size_t>(1, static_cast<std::size_t>(frameCount * parameters.keepFraction));

		if (report.peakFrame >= keepFrames) {
			report.result = DeverbResult::LatePeak;

			return report;
		}

		const std::size_t blockFrames = std::max<std::size_t>(1, SecondsToFrames(kBlockDuration, sampleRate));
		const std::size_t holdBlocks = SecondsToFrames(kFadeHoldDuration, sampleRate) / blockFrames;
		const std::vector<int> blockPeaks = MeasureBlockPeaks(samples, frameCount, channelCount, blockFrames);

		const double fadeLimit = peak * DecibelsToGain(-parameters.fadeThreshold);
		const std::size_t fadeStartBlock = FindFadeStartBlock(blockPeaks, report.peakFrame / blockFrames, fadeLimit, holdBlocks);
		const std::size_t maxFadeStartFrame = report.peakFrame + SecondsToFrames(parameters.maxFadeDelay, sampleRate);

		report.fadeStartFrame = std::min({fadeStartBlock * blockFrames, maxFadeStartFrame, keepFrames - 1});

		const double repeatLimit = peak * DecibelsToGain(-parameters.repeatLevel);
		const double gapLimit = peak * DecibelsToGain(-(parameters.repeatLevel + kRepeatGap));
		const std::size_t repeatBlock = FindRepeatBlock(blockPeaks, report.fadeStartFrame / blockFrames, repeatLimit, gapLimit);

		if (repeatBlock < blockPeaks.size()) {
			report.result = DeverbResult::RepeatedPeak;
			report.repeatFrame = repeatBlock * blockFrames;
			report.repeatLevel = GainToDecibels(blockPeaks[repeatBlock] / static_cast<double>(peak));

			return report;
		}

		const std::size_t decayFrames = SecondsToFrames(kDecayFloor / parameters.decayRate, sampleRate);
		report.endFrame = std::min(keepFrames, report.fadeStartFrame + std::max<std::size_t>(1, decayFrames));

		ApplyDecay(samples, frameCount, channelCount, sampleRate, report.fadeStartFrame, report.endFrame, parameters.decayRate);
		report.result = DeverbResult::Applied;

		return report;
	}

}
