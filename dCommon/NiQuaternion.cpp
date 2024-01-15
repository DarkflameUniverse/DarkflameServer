#include "NiQuaternion.h"

// C++
#include <cmath>

// MARK: Member Functions

Vector3 NiQuaternion::GetEulerAngles() const {
	Vector3 angles;

	// roll (x-axis rotation)
	const float sinr_cosp = 2 * (w * x + y * z);
	const float cosr_cosp = 1 - 2 * (x * x + y * y);
	angles.x = std::atan2(sinr_cosp, cosr_cosp);

	// pitch (y-axis rotation)
	const float sinp = 2 * (w * y - z * x);

	if (std::abs(sinp) >= 1) {
		angles.y = std::copysign(3.14 / 2, sinp); // use 90 degrees if out of range
	} else {
		angles.y = std::asin(sinp);
	}

	// yaw (z-axis rotation)
	const float siny_cosp = 2 * (w * z + x * y);
	const float cosy_cosp = 1 - 2 * (y * y + z * z);
	angles.z = std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

// MARK: Helper Functions

//! Look from a specific point in space to another point in space (Y-locked)
NiQuaternion NiQuaternion::LookAt(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
	//To make sure we don't orient around the X/Z axis:
	NiPoint3 source = sourcePoint;
	NiPoint3 dest = destPoint;
	source.y = 0.0f;
	dest.y = 0.0f;

	NiPoint3 forwardVector = NiPoint3(dest - source).Unitize();

	NiPoint3 posZ = NiPoint3::UNIT_Z;
	NiPoint3 vecA = posZ.CrossProduct(forwardVector).Unitize();

	float dot = posZ.DotProduct(forwardVector);
	float rotAngle = static_cast<float>(acos(dot));

	NiPoint3 vecB = vecA.CrossProduct(posZ);

	if (vecB.DotProduct(forwardVector) < 0) rotAngle = -rotAngle;
	return NiQuaternion::CreateFromAxisAngle(vecA, rotAngle);
}

//! Look from a specific point in space to another point in space
NiQuaternion NiQuaternion::LookAtUnlocked(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
	NiPoint3 forwardVector = NiPoint3(destPoint - sourcePoint).Unitize();

	NiPoint3 posZ = NiPoint3::UNIT_Z;
	NiPoint3 vecA = posZ.CrossProduct(forwardVector).Unitize();

	float dot = posZ.DotProduct(forwardVector);
	float rotAngle = static_cast<float>(acos(dot));

	NiPoint3 vecB = vecA.CrossProduct(posZ);

	if (vecB.DotProduct(forwardVector) < 0) rotAngle = -rotAngle;
	return NiQuaternion::CreateFromAxisAngle(vecA, rotAngle);
}

//! Creates a Quaternion from a specific axis and angle relative to that axis
NiQuaternion NiQuaternion::CreateFromAxisAngle(const Vector3& axis, float angle) {
	float halfAngle = angle * 0.5f;
	float s = static_cast<float>(sin(halfAngle));

	NiQuaternion q;
	q.x = axis.GetX() * s;
	q.y = axis.GetY() * s;
	q.z = axis.GetZ() * s;
	q.w = static_cast<float>(cos(halfAngle));

	return q;
}

NiQuaternion NiQuaternion::FromEulerAngles(const NiPoint3& eulerAngles) {
	// Abbreviations for the various angular functions
	float cy = cos(eulerAngles.z * 0.5);
	float sy = sin(eulerAngles.z * 0.5);
	float cp = cos(eulerAngles.y * 0.5);
	float sp = sin(eulerAngles.y * 0.5);
	float cr = cos(eulerAngles.x * 0.5);
	float sr = sin(eulerAngles.x * 0.5);

	NiQuaternion q;
	q.w = cr * cp * cy + sr * sp * sy;
	q.x = sr * cp * cy - cr * sp * sy;
	q.y = cr * sp * cy + sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;

	return q;
}
