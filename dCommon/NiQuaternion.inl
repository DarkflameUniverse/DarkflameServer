#pragma once
#ifndef __NIQUATERNION_H__
#error "This should only be included inline in NiQuaternion.h: Do not include directly!"
#endif

// Static Variables
constexpr const NiQuaternion NiQuaternion::IDENTITY(1, 0, 0, 0);

// MARK: Setters / Getters

//! Gets the W coordinate
constexpr float NiQuaternion::GetW() const noexcept {
	return this->w;
}

//! Sets the W coordinate
constexpr void NiQuaternion::SetW(const float w) noexcept {
	this->w = w;
}

//! Gets the X coordinate
constexpr float NiQuaternion::GetX() const noexcept {
	return this->x;
}

//! Sets the X coordinate
constexpr void NiQuaternion::SetX(const float x) noexcept {
	this->x = x;
}

//! Gets the Y coordinate
constexpr float NiQuaternion::GetY() const noexcept {
	return this->y;
}

//! Sets the Y coordinate
constexpr void NiQuaternion::SetY(const float y) noexcept {
	this->y = y;
}

//! Gets the Z coordinate
constexpr float NiQuaternion::GetZ() const noexcept {
	return this->z;
}

//! Sets the Z coordinate
constexpr void NiQuaternion::SetZ(const float z) noexcept {
	this->z = z;
}

// MARK: Member Functions

//! Returns the forward vector from the quaternion
constexpr Vector3 NiQuaternion::GetForwardVector() const noexcept {
	return Vector3(2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
}

//! Returns the up vector from the quaternion
constexpr Vector3 NiQuaternion::GetUpVector() const noexcept {
	return Vector3(2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x));
}

//! Returns the right vector from the quaternion
constexpr Vector3 NiQuaternion::GetRightVector() const noexcept {
	return Vector3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y));
}

// MARK: Operators

//! Operator to check for equality
constexpr bool NiQuaternion::operator==(const NiQuaternion& rot) const noexcept {
	return rot.x == this->x && rot.y == this->y && rot.z == this->z && rot.w == this->w;
}

//! Operator to check for inequality
constexpr bool NiQuaternion::operator!=(const NiQuaternion& rot) const noexcept {
	return !(*this == rot);
}
