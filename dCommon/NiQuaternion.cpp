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
	const float t2 = 2 * (w * y - z * x);
	angles.y = std::asin(std::clamp(t2, -1.0f, 1.0f)); // clamp to avoid NaN

	// if (std::abs(p) >= 1) {
	// 	angles.y = std::copysign(3.14 / 2, p); // use 90 degrees if out of range
	// } else {
	// 	angles.y = std::asin(p);
	// }

	// yaw (z-axis rotation)
	const float siny_cosp = 2 * (w * z + x * y);
	const float cosy_cosp = 1 - 2 * (y * y + z * z);
	angles.z = std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

NiQuaternion NiQuaternion::operator*(const float scalar) const noexcept {
	return NiQuaternion(this->w * scalar, this->x * scalar, this->y * scalar, this->z * scalar);
}

NiQuaternion& NiQuaternion::operator*=(const NiQuaternion& q) {
	auto& [ow, ox, oy, oz] = q;
	auto [cw, cx, cy, cz] = *this; // Current rotation copied because otherwise it screws up the math
	this->w = cw * ow - cx * ox - cy * oy - cz * oz;
	this->x = cw * ox + cx * ow + cy * oz - cz * oy;
	this->y = cw * oy + cy * ow + cz * ox - cx * oz;
	this->z = cw * oz + cz * ow + cx * oy - cy * ox;
	return *this;
}

NiQuaternion NiQuaternion::operator* (const NiQuaternion& q) const {
	auto& [ow, ox, oy, oz] = q;
	return NiQuaternion
	(
		/* w */w * ow - x * ox - y * oy - z * oz,
		/* x */w * ox + x * ow + y * oz - z * oy,
		/* y */w * oy + y * ow + z * ox - x * oz,
		/* z */w * oz + z * ow + x * oy - y * ox
	);
}

NiQuaternion NiQuaternion::operator/(const float& q) const noexcept {
	return NiQuaternion(this->w / q, this->x / q, this->y / q, this->z / q);
}

void NiQuaternion::Normalize() {
	float length = Dot(*this);
	float invLength = 1.0f / std::sqrt(length);
	*this = *this * invLength;
}

float NiQuaternion::Dot(const NiQuaternion& q) const noexcept {
	return (this->w * q.w) + (this->x * q.x) + (this->y * q.y) + (this->z * q.z);
}

void NiQuaternion::Inverse() noexcept {
	NiQuaternion copy = *this;
	copy.Conjugate();

	const float inv = 1.0f / Dot(*this);
	*this = copy / inv;
}

void NiQuaternion::Conjugate() noexcept {
	x = -x;
	y = -y;
	z = -z;
}

NiQuaternion NiQuaternion::Diff(const NiQuaternion& q) const noexcept {
	NiQuaternion inv = *this;
	inv.Inverse();
	return inv * q;
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

	NiPoint3 posZ = NiPoint3Constant::UNIT_Z;
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

	NiPoint3 posZ = NiPoint3Constant::UNIT_Z;
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
