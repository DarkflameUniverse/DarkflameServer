#pragma once

// Custom Classes
#include "NiPoint3.h"

/*!
  \file NiQuaternion.hpp
  \brief Defines a quaternion in space in WXYZ coordinates
 */

class NiQuaternion;
typedef NiQuaternion Quaternion;        //!< A typedef for a shorthand version of NiQuaternion

//! A class that defines a rotation in space
class NiQuaternion {
public:
	float w;            //!< The w coordinate
	float x;            //!< The x coordinate
	float y;            //!< The y coordinate
	float z;            //!< The z coordinate


	//! The initializer
	constexpr NiQuaternion() noexcept {
		this->w = 1;
		this->x = 0;
		this->y = 0;
		this->z = 0;
	}

	//! The initializer
	/*!
	  \param w The w coordinate
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	constexpr NiQuaternion(const float w, const float x, const float y, const float z) noexcept {
		this->w = w;
		this->x = x;
		this->y = y;
		this->z = z;
	}

	// MARK: Constants
	static const NiQuaternion IDENTITY;         //!< Quaternion(1, 0, 0, 0)

	// MARK: Setters / Getters

	//! Gets the W coordinate
	/*!
	  \return The w coordinate
	 */
	constexpr float GetW() const noexcept {
		return this->w;
	}

	//! Sets the W coordinate
	/*!
	  \param w The w coordinate
	 */
	constexpr void SetW(const float w) noexcept {
		this->w = w;
	}

	//! Gets the X coordinate
	/*!
	  \return The x coordinate
	 */
	constexpr float GetX() const noexcept {
		return this->x;
	}

	//! Sets the X coordinate
	/*!
	  \param x The x coordinate
	 */
	constexpr void SetX(const float x) noexcept {
		this->x = x;
	}

	//! Gets the Y coordinate
	/*!
	  \return The y coordinate
	 */
	constexpr float GetY() const noexcept {
		return this->y;
	}

	//! Sets the Y coordinate
	/*!
	  \param y The y coordinate
	 */
	constexpr void SetY(const float y) noexcept {
		this->y = y;
	}

	//! Gets the Z coordinate
	/*!
	  \return The z coordinate
	 */
	constexpr float GetZ() const noexcept {
		return this->z;
	}

	//! Sets the Z coordinate
	/*!
	  \param z The z coordinate
	 */
	constexpr void SetZ(const float z) noexcept {
		this->z = z;
	}


	// MARK: Member Functions

	//! Returns the forward vector from the quaternion
	/*!
	  \return The forward vector of the quaternion
	 */
	constexpr Vector3 GetForwardVector() const noexcept {
		return Vector3(2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
	}

	//! Returns the up vector from the quaternion
	/*!
	  \return The up vector fo the quaternion
	 */
	constexpr Vector3 GetUpVector() const noexcept {
		return Vector3(2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x));
	}

	//! Returns the right vector from the quaternion
	/*!
	  \return The right vector of the quaternion
	 */
	constexpr Vector3 GetRightVector() const noexcept {
		return Vector3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y));
	}

	constexpr Vector3 GetEulerAngles() const {
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


	// MARK: Operators

	//! Operator to check for equality
	constexpr bool operator==(const NiQuaternion& rot) const noexcept {
		return rot.x == this->x && rot.y == this->y && rot.z == this->z && rot.w == this->w;
	}

	//! Operator to check for inequality
	constexpr bool operator!=(const NiQuaternion& rot) const noexcept {
		return !(*this == rot);
	}

	// MARK: Helper Functions

	//! Look from a specific point in space to another point in space (Y-locked)
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	static constexpr NiQuaternion LookAt(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
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
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	static constexpr NiQuaternion LookAtUnlocked(const NiPoint3& sourcePoint, const NiPoint3& destPoint) {
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
	/*!
	  \param axis The axis that is used
	  \param angle The angle relative to this axis
	  \return A quaternion created from the axis and angle
	 */
	static constexpr NiQuaternion CreateFromAxisAngle(const Vector3& axis, float angle) {
		float halfAngle = angle * 0.5f;
		float s = static_cast<float>(sin(halfAngle));

		NiQuaternion q;
		q.x = axis.GetX() * s;
		q.y = axis.GetY() * s;
		q.z = axis.GetZ() * s;
		q.w = static_cast<float>(cos(halfAngle));

		return q;
	}

	static constexpr NiQuaternion FromEulerAngles(const NiPoint3& eulerAngles) {
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
};

// Static Variables
constexpr const NiQuaternion NiQuaternion::IDENTITY(1, 0, 0, 0);
