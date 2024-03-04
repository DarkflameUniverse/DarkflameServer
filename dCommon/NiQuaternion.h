#ifndef __NIQUATERNION_H__
#define __NIQUATERNION_H__

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
	float w{ 1 };            //!< The w coordinate
	float x{ 0 };            //!< The x coordinate
	float y{ 0 };            //!< The y coordinate
	float z{ 0 };            //!< The z coordinate


	//! The initializer
	constexpr NiQuaternion() = default;

	//! The initializer
	/*!
	  \param w The w coordinate
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	constexpr NiQuaternion(const float w, const float x, const float y, const float z) noexcept
		: w{ w }
		, x{ x }
		, y{ y }
		, z{ z } {
	}

	// MARK: Setters / Getters

	//! Gets the W coordinate
	/*!
	  \return The w coordinate
	 */
	[[nodiscard]] constexpr float GetW() const noexcept;

	//! Sets the W coordinate
	/*!
	  \param w The w coordinate
	 */
	constexpr void SetW(const float w) noexcept;

	//! Gets the X coordinate
	/*!
	  \return The x coordinate
	 */
	[[nodiscard]] constexpr float GetX() const noexcept;

	//! Sets the X coordinate
	/*!
	  \param x The x coordinate
	 */
	constexpr void SetX(const float x) noexcept;

	//! Gets the Y coordinate
	/*!
	  \return The y coordinate
	 */
	[[nodiscard]] constexpr float GetY() const noexcept;

	//! Sets the Y coordinate
	/*!
	  \param y The y coordinate
	 */
	constexpr void SetY(const float y) noexcept;

	//! Gets the Z coordinate
	/*!
	  \return The z coordinate
	 */
	[[nodiscard]] constexpr float GetZ() const noexcept;

	//! Sets the Z coordinate
	/*!
	  \param z The z coordinate
	 */
	constexpr void SetZ(const float z) noexcept;

	// MARK: Member Functions

	//! Returns the forward vector from the quaternion
	/*!
	  \return The forward vector of the quaternion
	 */
	[[nodiscard]] constexpr Vector3 GetForwardVector() const noexcept;

	//! Returns the up vector from the quaternion
	/*!
	  \return The up vector fo the quaternion
	 */
	[[nodiscard]] constexpr Vector3 GetUpVector() const noexcept;

	//! Returns the right vector from the quaternion
	/*!
	  \return The right vector of the quaternion
	 */
	[[nodiscard]] constexpr Vector3 GetRightVector() const noexcept;

	[[nodiscard]] Vector3 GetEulerAngles() const;

	// MARK: Operators

	//! Operator to check for equality
	constexpr bool operator==(const NiQuaternion& rot) const noexcept;

	//! Operator to check for inequality
	constexpr bool operator!=(const NiQuaternion& rot) const noexcept;

	// MARK: Helper Functions

	//! Look from a specific point in space to another point in space (Y-locked)
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	[[nodiscard]] static NiQuaternion LookAt(const NiPoint3& sourcePoint, const NiPoint3& destPoint);

	//! Look from a specific point in space to another point in space
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	[[nodiscard]] static NiQuaternion LookAtUnlocked(const NiPoint3& sourcePoint, const NiPoint3& destPoint);

	//! Creates a Quaternion from a specific axis and angle relative to that axis
	/*!
	  \param axis The axis that is used
	  \param angle The angle relative to this axis
	  \return A quaternion created from the axis and angle
	 */
	[[nodiscard]] static NiQuaternion CreateFromAxisAngle(const Vector3& axis, float angle);

	[[nodiscard]] static NiQuaternion FromEulerAngles(const NiPoint3& eulerAngles);
};

// Static Variables
namespace NiQuaternionConstant {
	constexpr NiQuaternion IDENTITY(1, 0, 0, 0);
}

// Include constexpr and inline function definitions in a seperate file for readability
#include "NiQuaternion.inl"

#endif // !__NIQUATERNION_H__
