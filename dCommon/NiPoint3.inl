#pragma once
#ifndef __NIPOINT3_H__
#error "This should only be included inline in NiPoint3.h: Do not include directly!"
#endif

#include "NiQuaternion.h"

// C++
#include <cmath>

// Static Variables
constexpr const NiPoint3 NiPoint3::ZERO(0.0f, 0.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_X(1.0f, 0.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_Y(0.0f, 1.0f, 0.0f);
constexpr const NiPoint3 NiPoint3::UNIT_Z(0.0f, 0.0f, 1.0f);
constexpr const NiPoint3 NiPoint3::UNIT_ALL(1.0f, 1.0f, 1.0f);

// MARK: Getters / Setters

//! Gets the X coordinate
constexpr float NiPoint3::GetX() const noexcept {
	return this->x;
}

//! Sets the X coordinate
constexpr void NiPoint3::SetX(const float x) noexcept {
	this->x = x;
}

//! Gets the Y coordinate
constexpr float NiPoint3::GetY() const noexcept {
	return this->y;
}

//! Sets the Y coordinate
constexpr void NiPoint3::SetY(const float y) noexcept {
	this->y = y;
}

//! Gets the Z coordinate
constexpr float NiPoint3::GetZ() const noexcept {
	return this->z;
}

//! Sets the Z coordinate
constexpr void NiPoint3::SetZ(const float z) noexcept {
	this->z = z;
}

// MARK: Member Functions

//! Gets the length of the vector
inline float NiPoint3::Length() const {
	return std::sqrt(x * x + y * y + z * z);
}

//! Gets the squared length of a vector
constexpr float NiPoint3::SquaredLength() const noexcept {
	return (x * x + y * y + z * z);
}

//! Returns the dot product of the vector dotted with another vector
constexpr float NiPoint3::DotProduct(const Vector3& vec) const noexcept {
	return ((this->x * vec.x) + (this->y * vec.y) + (this->z * vec.z));
}

//! Returns the cross product of the vector crossed with another vector
constexpr Vector3 NiPoint3::CrossProduct(const Vector3& vec) const noexcept {
	return Vector3(((this->y * vec.z) - (this->z * vec.y)),
		((this->z * vec.x) - (this->x * vec.z)),
		((this->x * vec.y) - (this->y * vec.x)));
}

//! Unitize the vector
inline NiPoint3 NiPoint3::Unitize() const {
	float length = this->Length();

	return length != 0 ? *this / length : NiPoint3::ZERO;
}

// MARK: Operators

//! Operator to check for equality
constexpr bool NiPoint3::operator==(const NiPoint3& point) const noexcept {
	return point.x == this->x && point.y == this->y && point.z == this->z;
}

//! Operator to check for inequality
constexpr bool NiPoint3::operator!=(const NiPoint3& point) const noexcept {
	return !(*this == point);
}

//! Operator for subscripting
constexpr float& NiPoint3::operator[](const int i) noexcept {
	float* base = &x;
	return base[i];
}

//! Operator for subscripting
constexpr const float& NiPoint3::operator[](const int i) const noexcept {
	const float* base = &x;
	return base[i];
}

//! Operator for addition of vectors
constexpr NiPoint3 NiPoint3::operator+(const NiPoint3& point) const noexcept {
	return NiPoint3(this->x + point.x, this->y + point.y, this->z + point.z);
}

//! Operator for addition of vectors
constexpr NiPoint3& NiPoint3::operator+=(const NiPoint3& point) noexcept {
	this->x += point.x;
	this->y += point.y;
	this->z += point.z;
	return *this;
}

constexpr NiPoint3& NiPoint3::operator*=(const float scalar) noexcept {
	this->x *= scalar;
	this->y *= scalar;
	this->z *= scalar;
	return *this;
}

//! Operator for subtraction of vectors
constexpr NiPoint3 NiPoint3::operator-(const NiPoint3& point) const noexcept {
	return NiPoint3(this->x - point.x, this->y - point.y, this->z - point.z);
}

//! Operator for addition of a scalar on all vector components
constexpr NiPoint3 NiPoint3::operator+(const float fScalar) const noexcept {
	return NiPoint3(this->x + fScalar, this->y + fScalar, this->z + fScalar);
}

//! Operator for subtraction of a scalar on all vector components
constexpr NiPoint3 NiPoint3::operator-(const float fScalar) const noexcept {
	return NiPoint3(this->x - fScalar, this->y - fScalar, this->z - fScalar);
}

//! Operator for scalar multiplication of a vector
constexpr NiPoint3 NiPoint3::operator*(const float fScalar) const noexcept {
	return NiPoint3(this->x * fScalar, this->y * fScalar, this->z * fScalar);
}

//! Operator for scalar division of a vector
constexpr NiPoint3 NiPoint3::operator/(const float fScalar) const noexcept {
	float retX = this->x != 0 ? this->x / fScalar : 0;
	float retY = this->y != 0 ? this->y / fScalar : 0;
	float retZ = this->z != 0 ? this->z / fScalar : 0;
	return NiPoint3(retX, retY, retZ);
}

// MARK: Helper Functions

//! Checks to see if the point (or vector) is with an Axis-Aligned Bounding Box
constexpr bool NiPoint3::IsWithinAxisAlignedBox(const NiPoint3& minPoint, const NiPoint3& maxPoint) noexcept {
	if (this->x < minPoint.x) return false;
	if (this->x > maxPoint.x) return false;
	if (this->y < minPoint.y) return false;
	if (this->y > maxPoint.y) return false;

	return (this->z < maxPoint.z && this->z > minPoint.z);
}

//! Checks to see if the point (or vector) is within a sphere
constexpr bool NiPoint3::IsWithinSphere(const NiPoint3& sphereCenter, const float radius) noexcept {
	Vector3 diffVec = Vector3(x - sphereCenter.GetX(), y - sphereCenter.GetY(), z - sphereCenter.GetZ());
	return (diffVec.SquaredLength() <= (radius * radius));
}

constexpr NiPoint3 NiPoint3::ClosestPointOnLine(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p) noexcept {
	if (a == b) return a;

	const auto pa = p - a;
	const auto ab = b - a;

	const auto t = pa.DotProduct(ab) / ab.SquaredLength();

	if (t <= 0.0f) return a;

	if (t >= 1.0f) return b;

	return a + ab * t;
}

inline float NiPoint3::Angle(const NiPoint3& a, const NiPoint3& b) {
	const auto dot = a.DotProduct(b);
	const auto lenA = a.SquaredLength();
	const auto lenB = a.SquaredLength();
	return acos(dot / sqrt(lenA * lenB));
}

inline float NiPoint3::Distance(const NiPoint3& a, const NiPoint3& b) {
	const auto dx = a.x - b.x;
	const auto dy = a.y - b.y;
	const auto dz = a.z - b.z;

	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

constexpr float NiPoint3::DistanceSquared(const NiPoint3& a, const NiPoint3& b) noexcept {
	const auto dx = a.x - b.x;
	const auto dy = a.y - b.y;
	const auto dz = a.z - b.z;

	return dx * dx + dy * dy + dz * dz;
}

inline NiPoint3 NiPoint3::MoveTowards(const NiPoint3& current, const NiPoint3& target, const float maxDistanceDelta) {
	float dx = target.x - current.x;
	float dy = target.y - current.y;
	float dz = target.z - current.z;

	float lengthSquared = static_cast<float>(
		static_cast<double>(dx) * static_cast<double>(dx) +
		static_cast<double>(dy) * static_cast<double>(dy) +
		static_cast<double>(dz) * static_cast<double>(dz)
		);

	if (static_cast<double>(lengthSquared) == 0.0
		|| static_cast<double>(maxDistanceDelta) >= 0.0
		&& static_cast<double>(lengthSquared)
		<= static_cast<double>(maxDistanceDelta) * static_cast<double>(maxDistanceDelta)) {
		return target;
	}

	float length = std::sqrt(lengthSquared);
	return NiPoint3(current.x + dx / length * maxDistanceDelta, current.y + dy / length * maxDistanceDelta, current.z + dz / length * maxDistanceDelta);
}

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
