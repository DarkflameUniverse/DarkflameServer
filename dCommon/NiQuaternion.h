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
	NiQuaternion(void);

	//! The initializer
	/*!
	  \param w The w coordinate
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	NiQuaternion(float w, float x, float y, float z);

	//! Destructor
	~NiQuaternion(void);

	// MARK: Constants
	static const NiQuaternion IDENTITY;         //!< Quaternion(1, 0, 0, 0)

	// MARK: Setters / Getters

	//! Gets the W coordinate
	/*!
	  \return The w coordinate
	 */
	float GetW(void) const;

	//! Sets the W coordinate
	/*!
	  \param w The w coordinate
	 */
	void SetW(float w);

	//! Gets the X coordinate
	/*!
	  \return The x coordinate
	 */
	float GetX(void) const;

	//! Sets the X coordinate
	/*!
	  \param x The x coordinate
	 */
	void SetX(float x);

	//! Gets the Y coordinate
	/*!
	  \return The y coordinate
	 */
	float GetY(void) const;

	//! Sets the Y coordinate
	/*!
	  \param y The y coordinate
	 */
	void SetY(float y);

	//! Gets the Z coordinate
	/*!
	  \return The z coordinate
	 */
	float GetZ(void) const;

	//! Sets the Z coordinate
	/*!
	  \param z The z coordinate
	 */
	void SetZ(float z);


	// MARK: Member Functions

	//! Returns the forward vector from the quaternion
	/*!
	  \return The forward vector of the quaternion
	 */
	Vector3 GetForwardVector(void) const;

	//! Returns the up vector from the quaternion
	/*!
	  \return The up vector fo the quaternion
	 */
	Vector3 GetUpVector(void) const;

	//! Returns the right vector from the quaternion
	/*!
	  \return The right vector of the quaternion
	 */
	Vector3 GetRightVector(void) const;

	Vector3 GetEulerAngles() const;


	// MARK: Operators

	//! Operator to check for equality
	bool operator==(const NiQuaternion& rot) const;

	//! Operator to check for inequality
	bool operator!=(const NiQuaternion& rot) const;


	// MARK: Helper Functions

	//! Look from a specific point in space to another point in space (Y-locked)
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	static NiQuaternion LookAt(const NiPoint3& sourcePoint, const NiPoint3& destPoint);

	//! Look from a specific point in space to another point in space
	/*!
	  \param sourcePoint The source location
	  \param destPoint The destination location
	  \return The Quaternion with the rotation towards the destination
	 */
	static NiQuaternion LookAtUnlocked(const NiPoint3& sourcePoint, const NiPoint3& destPoint);

	//! Creates a Quaternion from a specific axis and angle relative to that axis
	/*!
	  \param axis The axis that is used
	  \param angle The angle relative to this axis
	  \return A quaternion created from the axis and angle
	 */
	static NiQuaternion CreateFromAxisAngle(const Vector3& axis, float angle);

	static NiQuaternion FromEulerAngles(const NiPoint3& eulerAngles);
};
