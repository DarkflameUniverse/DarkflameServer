#ifndef __NIPOINT3_H__
#define __NIPOINT3_H__

/*!
  \file NiPoint3.hpp
  \brief Defines a point in space in XYZ coordinates
 */

class NiPoint3;
class NiQuaternion;
typedef NiPoint3 Vector3;       //!< The Vector3 class is technically the NiPoint3 class, but typedef'd for clarity in some cases

//! A custom class the defines a point in space
class NiPoint3 {
public:
	float x;            //!< The x position
	float y;            //!< The y position
	float z;            //!< The z position


	//! Initializer
	constexpr NiPoint3() noexcept
		: x{ 0 }
		, y{ 0 }
		, z{ 0 } {
	}

	//! Initializer
	/*!
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	constexpr NiPoint3(const float x, const float y, const float z) noexcept
		: x{ x }
		, y{ y }
		, z{ z } {
	}

	//! Copy Constructor
	/*!
	  \param point The point to copy
	 */
	constexpr NiPoint3(const NiPoint3& point) noexcept
		: x{ point.x }
		, y{ point.y }
		, z{ point.z } {
	}

	// MARK: Constants
	static NiPoint3 ZERO;         //!< Point(0, 0, 0)
	static NiPoint3 UNIT_X;       //!< Point(1, 0, 0)
	static NiPoint3 UNIT_Y;       //!< Point(0, 1, 0)
	static NiPoint3 UNIT_Z;       //!< Point(0, 0, 1)
	static NiPoint3 UNIT_ALL;     //!< Point(1, 1, 1)

	// MARK: Getters / Setters

	//! Gets the X coordinate
	/*!
	  \return The x coordinate
	 */
	constexpr float GetX() const noexcept;

	//! Sets the X coordinate
	/*!
	  \param x The x coordinate
	 */
	constexpr void SetX(const float x) noexcept;

	//! Gets the Y coordinate
	/*!
	  \return The y coordinate
	 */
	constexpr float GetY() const noexcept;

	//! Sets the Y coordinate
	/*!
	  \param y The y coordinate
	 */
	constexpr void SetY(const float y) noexcept;

	//! Gets the Z coordinate
	/*!
	  \return The z coordinate
	 */
	constexpr float GetZ() const noexcept;

	//! Sets the Z coordinate
	/*!
	  \param z The z coordinate
	 */
	constexpr void SetZ(const float z) noexcept;

	// MARK: Member Functions

	//! Gets the length of the vector
	/*!
	  \return The scalar length of the vector
	 */
	float Length() const;

	//! Gets the squared length of a vector
	/*!
	  \return The squared length of a vector
	 */
	constexpr float SquaredLength() const noexcept;

	//! Returns the dot product of the vector dotted with another vector
	/*!
	  \param vec The second vector
	  \return The dot product of the two vectors
	 */
	constexpr float DotProduct(const Vector3& vec) const noexcept;

	//! Returns the cross product of the vector crossed with another vector
	/*!
	  \param vec The second vector
	  \return The cross product of the two vectors
	 */
	constexpr Vector3 CrossProduct(const Vector3& vec) const noexcept;

	//! Unitize the vector
	/*!
	  \returns The current vector
	 */
	NiPoint3 Unitize() const;

	// MARK: Operators

	//! Operator to check for equality
	constexpr bool operator==(const NiPoint3& point) const noexcept;

	//! Operator to check for inequality
	constexpr bool operator!=(const NiPoint3& point) const noexcept;

	//! Operator for subscripting
	constexpr float& operator[](const int i) noexcept;

	//! Operator for subscripting
	constexpr const float& operator[](const int i) const noexcept;

	//! Operator for addition of vectors
	constexpr NiPoint3 operator+(const NiPoint3& point) const noexcept;

	//! Operator for addition of vectors
	constexpr NiPoint3& operator+=(const NiPoint3& point) noexcept;

	constexpr NiPoint3& operator*=(const float scalar) noexcept;

	//! Operator for subtraction of vectors
	constexpr NiPoint3 operator-(const NiPoint3& point) const noexcept;

	//! Operator for addition of a scalar on all vector components
	constexpr NiPoint3 operator+(const float fScalar) const noexcept;

	//! Operator for subtraction of a scalar on all vector components
	constexpr NiPoint3 operator-(const float fScalar) const noexcept;

	//! Operator for scalar multiplication of a vector
	constexpr NiPoint3 operator*(const float fScalar) const noexcept;

	//! Operator for scalar division of a vector
	constexpr NiPoint3 operator/(const float fScalar) const noexcept;

	// MARK: Helper Functions

	//! Checks to see if the point (or vector) is with an Axis-Aligned Bounding Box
	/*!
	  \param minPoint The minimum point of the bounding box
	  \param maxPoint The maximum point of the bounding box
	  \return Whether or not this point lies within the box
	 */
	constexpr bool IsWithinAxisAlignedBox(const NiPoint3& minPoint, const NiPoint3& maxPoint) noexcept;

	//! Checks to see if the point (or vector) is within a sphere
	/*!
	  \param sphereCenter The sphere center
	  \param radius The radius
	 */
	constexpr bool IsWithinSphere(const NiPoint3& sphereCenter, const float radius) noexcept;

	/*!
	  \param a Start of line
	  \param b End of line
	  \param p Refrence point
	  \return The point of line AB which is closest to P
	*/
	static constexpr NiPoint3 ClosestPointOnLine(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p) noexcept;

	static float Angle(const NiPoint3& a, const NiPoint3& b);

	static float Distance(const NiPoint3& a, const NiPoint3& b);

	static constexpr float DistanceSquared(const NiPoint3& a, const NiPoint3& b) noexcept;

	static NiPoint3 MoveTowards(const NiPoint3& current, const NiPoint3& target, const float maxDistanceDelta);

	//This code is yoinked from the MS XNA code, so it should be right, even if it's horrible.
	constexpr NiPoint3 RotateByQuaternion(const NiQuaternion& rotation) noexcept;
};

// .inl file needed for code organization and to circumvent circular dependency issues
#include "NiPoint3.inl"

#endif // !__NIPOINT3_H__
