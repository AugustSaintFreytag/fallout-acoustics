#pragma once

#include <cmath>

namespace sea {

	// 3-dimensional vector type.
	// Follows same layout as `NiPoint3` and `NiVector3` (3 float properties), compatible with engine objects.
	struct Vector3 {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	inline Vector3 operator+(const Vector3& left, const Vector3& right) {
		return {left.x + right.x, left.y + right.y, left.z + right.z};
	}

	inline Vector3 operator-(const Vector3& left, const Vector3& right) {
		return {left.x - right.x, left.y - right.y, left.z - right.z};
	}

	inline Vector3 operator*(const Vector3& vector, float factor) {
		return {vector.x * factor, vector.y * factor, vector.z * factor};
	}

	inline float Length(const Vector3& vector) {
		return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	}

	inline float Distance(const Vector3& from, const Vector3& to) {
		return Length(to - from);
	}

	// Returns the vector scaled to unit length, or a zero vector for a zero-length input.
	inline Vector3 Normalize(const Vector3& vector) {
		const float length = Length(vector);

		if (length <= 0.0f) {
			return {};
		}

		return vector * (1.0f / length);
	}

}
