#pragma once

// C++
#include <cmath>

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
	constexpr NiPoint3() noexcept {
		this->x = 0;
		this->y = 0;
		this->z = 0;
	}

	//! Initializer
	/*!
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	constexpr NiPoint3(const float x, const float y, const float z) noexcept {
		this->x = x;
		this->y = y;
		this->z = z;
	}

	//! Copy Constructor
	/*!
	  \param point The point to copy
	 */
	constexpr NiPoint3(const NiPoint3& point) noexcept {
		this->x = point.x;
		this->y = point.y;
		this->z = point.z;
	}

	// MARK: Constants
	static const NiPoint3 ZERO;         //!< Point(0, 0, 0)
	static const NiPoint3 UNIT_X;       //!< Point(1, 0, 0)
	static const NiPoint3 UNIT_Y;       //!< Point(0, 1, 0)
	static const NiPoint3 UNIT_Z;       //!< Point(0, 0, 1)
	static const NiPoint3 UNIT_ALL;     //!< Point(1, 1, 1)

	// MARK: Getters / Setters

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

	//! Gets the length of the vector
	/*!
	  \return The scalar length of the vector
	 */
	constexpr float Length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	//! Gets the squared length of a vector
	/*!
	  \return The squared length of a vector
	 */
	constexpr float SquaredLength() const noexcept {
		return (x * x + y * y + z * z);
	}

	//! Returns the dot product of the vector dotted with another vector
	/*!
	  \param vec The second vector
	  \return The dot product of the two vectors
	 */
	constexpr float DotProduct(const Vector3& vec) const noexcept {
		return ((this->x * vec.x) + (this->y * vec.y) + (this->z * vec.z));
	}

	//! Returns the cross product of the vector crossed with another vector
	/*!
	  \param vec The second vector
	  \return The cross product of the two vectors
	 */
	constexpr Vector3 CrossProduct(const Vector3& vec) const noexcept {
		return Vector3(((this->y * vec.z) - (this->z * vec.y)),
			((this->z * vec.x) - (this->x * vec.z)),
			((this->x * vec.y) - (this->y * vec.x)));
	}

	//! Unitize the vector
	/*!
	  \returns The current vector
	 */
	constexpr NiPoint3 Unitize() const {
		float length = this->Length();

		return length != 0 ? *this / length : NiPoint3::ZERO;
	}

	// MARK: Operators

	//! Operator to check for equality
	constexpr bool operator==(const NiPoint3& point) const noexcept {
		return point.x == this->x && point.y == this->y && point.z == this->z;
	}

	//! Operator to check for inequality
	constexpr bool operator!=(const NiPoint3& point) const noexcept {
		return !(*this == point);
	}

	//! Operator for subscripting
	constexpr float& operator[](const int i) noexcept {
		float* base = &x;
		return base[i];
	}

	//! Operator for subscripting
	constexpr const float& operator[](const int i) const noexcept { // TODO: Is this redundant?
		const float* base = &x;
		return base[i];
	}

	//! Operator for addition of vectors
	constexpr NiPoint3 operator+(const NiPoint3& point) const noexcept {
		return NiPoint3(this->x + point.x, this->y + point.y, this->z + point.z);
	}

	//! Operator for addition of vectors
	constexpr NiPoint3& operator+=(const NiPoint3& point) noexcept {
		this->x += point.x;
		this->y += point.y;
		this->z += point.z;
		return *this;
	}

	constexpr NiPoint3& operator*=(const float scalar) noexcept {
		this->x *= scalar;
		this->y *= scalar;
		this->z *= scalar;
		return *this;
	}

	//! Operator for subtraction of vectors
	constexpr NiPoint3 operator-(const NiPoint3& point) const noexcept {
		return NiPoint3(this->x - point.x, this->y - point.y, this->z - point.z);
	}

	//! Operator for addition of a scalar on all vector components
	constexpr NiPoint3 operator+(const float fScalar) const noexcept {
		return NiPoint3(this->x + fScalar, this->y + fScalar, this->z + fScalar);
	}

	//! Operator for subtraction of a scalar on all vector components
	constexpr NiPoint3 operator-(const float fScalar) const noexcept {
		return NiPoint3(this->x - fScalar, this->y - fScalar, this->z - fScalar);
	}

	//! Operator for scalar multiplication of a vector
	constexpr NiPoint3 operator*(const float fScalar) const noexcept {
		return NiPoint3(this->x * fScalar, this->y * fScalar, this->z * fScalar);
	}

	//! Operator for scalar division of a vector
	constexpr NiPoint3 operator/(const float fScalar) const noexcept {
		float retX = this->x != 0 ? this->x / fScalar : 0;
		float retY = this->y != 0 ? this->y / fScalar : 0;
		float retZ = this->z != 0 ? this->z / fScalar : 0;
		return NiPoint3(retX, retY, retZ);
	}

	// MARK: Helper Functions

	//! Checks to see if the point (or vector) is with an Axis-Aligned Bounding Box
	/*!
	  \param minPoint The minimum point of the bounding box
	  \param maxPoint The maximum point of the bounding box
	  \return Whether or not this point lies within the box
	 */
	constexpr bool IsWithinAxisAlignedBox(const NiPoint3& minPoint, const NiPoint3& maxPoint) noexcept {
		if (this->x < minPoint.x) return false;
		if (this->x > maxPoint.x) return false;
		if (this->y < minPoint.y) return false;
		if (this->y > maxPoint.y) return false;

		return (this->z < maxPoint.z && this->z > minPoint.z);
	}

	//! Checks to see if the point (or vector) is within a sphere
	/*!
	  \param sphereCenter The sphere center
	  \param radius The radius
	 */
	constexpr bool IsWithinSphere(const NiPoint3& sphereCenter, const float radius) noexcept {
		Vector3 diffVec = Vector3(x - sphereCenter.GetX(), y - sphereCenter.GetY(), z - sphereCenter.GetZ());
		return (diffVec.SquaredLength() <= (radius * radius));
	}

	/*!
	  \param a Start of line
	  \param b End of line
	  \param p Refrence point
	  \return The point of line AB which is closest to P
	*/
	static constexpr NiPoint3 ClosestPointOnLine(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p) noexcept {
		if (a == b) return a;

		const auto pa = p - a;
		const auto ab = b - a;

		const auto t = pa.DotProduct(ab) / ab.SquaredLength();

		if (t <= 0.0f) return a;

		if (t >= 1.0f) return b;

		return a + ab * t;
	}

	static constexpr float Angle(const NiPoint3& a, const NiPoint3& b) {
		const auto dot = a.DotProduct(b);
		const auto lenA = a.SquaredLength();
		const auto lenB = a.SquaredLength();
		return acos(dot / sqrt(lenA * lenB));
	}

	static constexpr float Distance(const NiPoint3& a, const NiPoint3& b) {
		const auto dx = a.x - b.x;
		const auto dy = a.y - b.y;
		const auto dz = a.z - b.z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	static constexpr float DistanceSquared(const NiPoint3& a, const NiPoint3& b) noexcept {
		const auto dx = a.x - b.x;
		const auto dy = a.y - b.y;
		const auto dz = a.z - b.z;

		return dx * dx + dy * dy + dz * dz;
	}

	static constexpr NiPoint3 MoveTowards(const NiPoint3& current, const NiPoint3& target, const float maxDistanceDelta) {
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
	constexpr NiPoint3 RotateByQuaternion(const NiQuaternion& rotation) noexcept;
};

// .inl file needed to circumvent circular dependency issues
#include "NiPoint3.inl"
