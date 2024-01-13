#pragma once

#ifndef __NIPOINT3_H__
#error "This should only be included inline in NiPoint3.h: Do not include directly!"
#endif

// This code is here to get around circular dependency issues
#include "NiQuaternion.h"

// Static Variables
constexpr const NiPoint3 NiPoint3::ZERO(0.0f, 0.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_X(1.0f, 0.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_Y(0.0f, 1.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_Z(0.0f, 0.0f, 1.0f);
constexpr const NiPoint3 NiPoint3::UNIT_ALL(1.0f, 1.0f, 1.0f);

//This code is yoinked from the MS XNA code, so it should be right, even if it's horrible.
constexpr NiPoint3 NiPoint3::RotateByQuaternion(const NiQuaternion& rotation) noexcept {
	Vector3 vector;
	float num12 = rotation.x + rotation.x;
	float num2 = rotation.y + rotation.y;
	float num = rotation.z + rotation.z;
	float num11 = rotation.w * num12;
	float num10 = rotation.w * num2;
	float num9 = rotation.w * num;
	float num8 = rotation.x * num12;
	float num7 = rotation.x * num2;
	float num6 = rotation.x * num;
	float num5 = rotation.y * num2;
	float num4 = rotation.y * num;
	float num3 = rotation.z * num;

	NiPoint3 value = *this;
	float num15 = ((value.x * ((1.0f - num5) - num3)) + (value.y * (num7 - num9))) + (value.z * (num6 + num10));
	float num14 = ((value.x * (num7 + num9)) + (value.y * ((1.0f - num8) - num3))) + (value.z * (num4 - num11));
	float num13 = ((value.x * (num6 - num10)) + (value.y * (num4 + num11))) + (value.z * ((1.0f - num8) - num5));
	vector.x = num15;
	vector.y = num14;
	vector.z = num13;
	return vector;
}
