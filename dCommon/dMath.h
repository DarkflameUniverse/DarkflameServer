// Darkflame Universe
// Copyright 2025

#ifndef DMATH_H
#define DMATH_H

#include <cmath>

namespace Math {
	constexpr float	PI = 3.14159265358979323846264338327950288f;
	constexpr float RATIO_DEG_TO_RAD = PI / 180.0f;
	constexpr float RATIO_RAD_TO_DEG = 180.0f / PI;

	inline float DegToRad(float degrees) {
		return degrees * RATIO_DEG_TO_RAD;
	}

	inline float RadToDeg(float radians) {
		return radians * RATIO_RAD_TO_DEG;
	}
};

#endif //!DMATH_H
