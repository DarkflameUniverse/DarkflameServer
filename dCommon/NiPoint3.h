#pragma once

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
	NiPoint3(void);

	//! Initializer
	/*!
	  \param x The x coordinate
	  \param y The y coordinate
	  \param z The z coordinate
	 */
	NiPoint3(float x, float y, float z);

	//! Copy Constructor
	/*!
	  \param point The point to copy
	 */
	NiPoint3(const NiPoint3& point);

	//! Destructor
	~NiPoint3(void);

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

	//! Gets the length of the vector
	/*!
	  \return The scalar length of the vector
	 */
	float Length(void) const;

	//! Gets the squared length of a vector
	/*!
	  \return The squared length of a vector
	 */
	float SquaredLength(void) const;

	//! Returns the dot product of the vector dotted with another vector
	/*!
	  \param vec The second vector
	  \return The dot product of the two vectors
	 */
	float DotProduct(const Vector3& vec) const;

	//! Returns the cross product of the vector crossed with another vector
	/*!
	  \param vec The second vector
	  \return The cross product of the two vectors
	 */
	Vector3 CrossProduct(const Vector3& vec) const;

	//! Unitize the vector
	/*!
	  \returns The current vector
	 */
	NiPoint3 Unitize(void) const;


	// MARK: Operators

	//! Operator to check for equality
	bool operator==(const NiPoint3& point) const;

	//! Operator to check for inequality
	bool operator!=(const NiPoint3& point) const;

	//! Operator for subscripting
	float& operator[](int i);

	//! Operator for subscripting
	const float& operator[](int i) const;

	//! Operator for addition of vectors
	NiPoint3 operator+(const NiPoint3& point) const;

	//! Operator for subtraction of vectors
	NiPoint3 operator-(const NiPoint3& point) const;

	//! Operator for addition of a scalar on all vector components
	NiPoint3 operator+(float fScalar) const;

	//! Operator for subtraction of a scalar on all vector components
	NiPoint3 operator-(float fScalar) const;

	//! Operator for scalar multiplication of a vector
	NiPoint3 operator*(float fScalar) const;

	//! Operator for scalar division of a vector
	NiPoint3 operator/(float fScalar) const;


	// MARK: Helper Functions

	//! Checks to see if the point (or vector) is with an Axis-Aligned Bounding Box
	/*!
	  \param minPoint The minimum point of the bounding box
	  \param maxPoint The maximum point of the bounding box
	  \return Whether or not this point lies within the box
	 */
	bool IsWithinAxisAlignedBox(const NiPoint3& minPoint, const NiPoint3& maxPoint);

	//! Checks to see if the point (or vector) is within a sphere
	/*!
	  \param sphereCenter The sphere center
	  \param radius The radius
	 */
	bool IsWithinSpehere(const NiPoint3& sphereCenter, float radius);

	/*!
	  \param a Start of line
	  \param b End of line
	  \param p Refrence point
	  \return The point of line AB which is closest to P
	*/
	static NiPoint3 ClosestPointOnLine(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p);

	static float Angle(const NiPoint3& a, const NiPoint3& b);

	static float Distance(const NiPoint3& a, const NiPoint3& b);

	static float DistanceSquared(const NiPoint3& a, const NiPoint3& b);

	static NiPoint3 MoveTowards(const NiPoint3& current, const NiPoint3& target, float maxDistanceDelta);

	NiPoint3 RotateByQuaternion(const NiQuaternion& rotation);
};
