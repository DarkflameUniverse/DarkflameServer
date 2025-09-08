#include "NiQuaternion.h"

// C++
#include <cmath>

#include <glm/gtx/quaternion.hpp>

// MARK: Member Functions

Vector3 QuatUtils::Euler(const NiQuaternion& quat) {
	return glm::eulerAngles(quat);
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
NiQuaternion QuatUtils::LookAt(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
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
	return glm::angleAxis(rotAngle, glm::vec3{vecA.x, vecA.y, vecA.z});
}

//! Look from a specific point in space to another point in space
NiQuaternion QuatUtils::LookAtUnlocked(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
	NiPoint3 forwardVector = NiPoint3(destPoint - sourcePoint).Unitize();

	NiPoint3 posZ = NiPoint3Constant::UNIT_Z;
	NiPoint3 vecA = posZ.CrossProduct(forwardVector).Unitize();

	float dot = posZ.DotProduct(forwardVector);
	float rotAngle = static_cast<float>(acos(dot));

	NiPoint3 vecB = vecA.CrossProduct(posZ);

	if (vecB.DotProduct(forwardVector) < 0) rotAngle = -rotAngle;
	return glm::angleAxis(rotAngle, glm::vec3{vecA.x, vecA.y, vecA.z});
}

//! Creates a Quaternion from a specific axis and angle relative to that axis
NiQuaternion QuatUtils::AxisAngle(const Vector3& axis, float angle) {
	return glm::angleAxis(angle, glm::vec3(axis.x, axis.y, axis.z));
}

NiQuaternion QuatUtils::FromEuler(const NiPoint3& eulerAngles) {
	return glm::quat(glm::vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z));
}

Vector3 QuatUtils::Forward(const NiQuaternion& quat) {
	return quat * glm::vec3(0, 0, 1);
}

Vector3 QuatUtils::Up(const NiQuaternion& quat) {
	return quat * glm::vec3(0, 1, 0);
}

Vector3 QuatUtils::Right(const NiQuaternion& quat) {
	return quat * glm::vec3(1, 0, 0);
}
