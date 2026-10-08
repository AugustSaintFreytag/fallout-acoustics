#pragma once

#include <cmath>
#include <numbers>

namespace sea::effects {

	// Second-Order IIR Filter
	// Coefficients yoinked from the RBJ Audio EQ Cookbook.
	struct Biquad {
		double b0 = 1.0;
		double b1 = 0.0;
		double b2 = 0.0;
		double a1 = 0.0;
		double a2 = 0.0;

		double state1 = 0.0;
		double state2 = 0.0;

		static Biquad HighPass(double sampleRate, double frequency, double q) {
			const Shape shape = MakeShape(sampleRate, frequency, q);
			const double edge = (1.0 + shape.cosine) / 2.0;

			return Normalized(edge, -(1.0 + shape.cosine), edge, 1.0 + shape.alpha, -2.0 * shape.cosine, 1.0 - shape.alpha);
		}

		static Biquad LowPass(double sampleRate, double frequency, double q) {
			const Shape shape = MakeShape(sampleRate, frequency, q);
			const double edge = (1.0 - shape.cosine) / 2.0;

			return Normalized(edge, 1.0 - shape.cosine, edge, 1.0 + shape.alpha, -2.0 * shape.cosine, 1.0 - shape.alpha);
		}

		static Biquad Peaking(double sampleRate, double frequency, double q, double gainDb) {
			const Shape shape = MakeShape(sampleRate, frequency, q);
			const double amplitude = std::pow(10.0, gainDb / 40.0);

			return Normalized(1.0 + shape.alpha * amplitude, -2.0 * shape.cosine, 1.0 - shape.alpha * amplitude,
				1.0 + shape.alpha / amplitude, -2.0 * shape.cosine, 1.0 - shape.alpha / amplitude);
		}

		// Filters one sample. Transposed direct form II.
		double Process(double input) {
			const double output = b0 * input + state1;
			state1 = b1 * input - a1 * output + state2;
			state2 = b2 * input - a2 * output;

			return output;
		}

	private:
	
		struct Shape {
			double cosine;
			double alpha;
		};

		static Shape MakeShape(double sampleRate, double frequency, double q) {
			const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;

			return {std::cos(omega), std::sin(omega) / (2.0 * q)};
		}

		static Biquad Normalized(double feedforward0, double feedforward1, double feedforward2, double feedback0,
			double feedback1, double feedback2) {
			Biquad filter;
			filter.b0 = feedforward0 / feedback0;
			filter.b1 = feedforward1 / feedback0;
			filter.b2 = feedforward2 / feedback0;
			filter.a1 = feedback1 / feedback0;
			filter.a2 = feedback2 / feedback0;

			return filter;
		}
	};

}
