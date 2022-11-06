#include "NiPoint3.h"
#include "NiQuaternion.h"

// C++
#include <cmath>

// Static Variables
const NiPoint3 NiPoint3::ZERO(0.0f, 0.0f, 0.0f);
const NiPoint3 NiPoint3::UNIT_X(1.0f, 0.0f, 0.0f);
const NiPoint3 NiPoint3::UNIT_Y(0.0f, 1.0f, 0.0f);
const NiPoint3 NiPoint3::UNIT_Z(0.0f, 0.0f, 1.0f);
const NiPoint3 NiPoint3::UNIT_ALL(1.0f, 1.0f, 1.0f);

//! Initializer
NiPoint3::NiPoint3(void) {
	this->x = 0;
	this->y = 0;
	this->z = 0;
}

//! Initializer
NiPoint3::NiPoint3(float x, float y, float z) {
	this->x = x;
	this->y = y;
	this->z = z;
}

//! Copy Constructor
NiPoint3::NiPoint3(const NiPoint3& point) {
	this->x = point.x;
	this->y = point.y;
	this->z = point.z;
}

//! Destructor
NiPoint3::~NiPoint3(void) {}

// MARK: Getters / Setters

//! Gets the X coordinate
float NiPoint3::GetX(void) const {
	return this->x;
}

//! Sets the X coordinate
void NiPoint3::SetX(float x) {
	this->x = x;
}

//! Gets the Y coordinate
float NiPoint3::GetY(void) const {
	return this->y;
}

//! Sets the Y coordinate
void NiPoint3::SetY(float y) {
	this->y = y;
}

//! Gets the Z coordinate
float NiPoint3::GetZ(void) const {
	return this->z;
}

//! Sets the Z coordinate
void NiPoint3::SetZ(float z) {
	this->z = z;
}

// MARK: Functions

//! Gets the length of the vector
float NiPoint3::Length(void) const {
	return sqrt(x * x + y * y + z * z);
}

//! Gets the squared length of a vector
float NiPoint3::SquaredLength(void) const {
	return (x * x + y * y + z * z);
}

//! Returns the dot product of the vector dotted with another vector
float NiPoint3::DotProduct(const Vector3& vec) const {
	return ((this->x * vec.x) + (this->y * vec.y) + (this->z * vec.z));
}

//! Returns the cross product of the vector crossed with another vector
Vector3 NiPoint3::CrossProduct(const Vector3& vec) const {
	return Vector3(((this->y * vec.z) - (this->z * vec.y)),
		((this->z * vec.x) - (this->x * vec.z)),
		((this->x * vec.y) - (this->y * vec.x)));
}

//! Unitize the vector
NiPoint3 NiPoint3::Unitize(void) const {
	float length = this->Length();

	return length != 0 ? *this / length : NiPoint3::ZERO;
}


// MARK: Operators

//! Operator to check for equality
bool NiPoint3::operator==(const NiPoint3& point) const {
	return point.x == this->x && point.y == this->y && point.z == this->z;
}

//! Operator to check for inequality
bool NiPoint3::operator!=(const NiPoint3& point) const {
	return !(*this == point);
}

//! Operator for subscripting
float& NiPoint3::operator[](int i) {
	float* base = &x;
	return (float&)base[i];
}

//! Operator for subscripting
const float& NiPoint3::operator[](int i) const {
	const float* base = &x;
	return (float&)base[i];
}

//! Operator for addition of vectors
NiPoint3 NiPoint3::operator+(const NiPoint3& point) const {
	return NiPoint3(this->x + point.x, this->y + point.y, this->z + point.z);
}

//! Operator for subtraction of vectors
NiPoint3 NiPoint3::operator-(const NiPoint3& point) const {
	return NiPoint3(this->x - point.x, this->y - point.y, this->z - point.z);
}

//! Operator for addition of a scalar on all vector components
NiPoint3 NiPoint3::operator+(float fScalar) const {
	return NiPoint3(this->x + fScalar, this->y + fScalar, this->z + fScalar);
}

//! Operator for subtraction of a scalar on all vector components
NiPoint3 NiPoint3::operator-(float fScalar) const {
	return NiPoint3(this->x - fScalar, this->y - fScalar, this->z - fScalar);
}

//! Operator for scalar multiplication of a vector
NiPoint3 NiPoint3::operator*(float fScalar) const {
	return NiPoint3(this->x * fScalar, this->y * fScalar, this->z * fScalar);
}

//! Operator for scalar division of a vector
NiPoint3 NiPoint3::operator/(float fScalar) const {
	float retX = this->x != 0 ? this->x / fScalar : 0;
	float retY = this->y != 0 ? this->y / fScalar : 0;
	float retZ = this->z != 0 ? this->z / fScalar : 0;
	return NiPoint3(retX, retY, retZ);
}


// MARK: Helper Functions

//! Checks to see if the point (or vector) is with an Axis-Aligned Bounding Box
bool NiPoint3::IsWithinAxisAlignedBox(const NiPoint3& minPoint, const NiPoint3& maxPoint) {
	if (this->x < minPoint.x) return false;
	if (this->x > maxPoint.x) return false;
	if (this->y < minPoint.y) return false;
	if (this->y > maxPoint.y) return false;

	return (this->z < maxPoint.z&& this->z > minPoint.z);
}

//! Checks to see if the point (or vector) is within a sphere
bool NiPoint3::IsWithinSpehere(const NiPoint3& sphereCenter, float radius) {
	Vector3 diffVec = Vector3(x - sphereCenter.GetX(), y - sphereCenter.GetY(), z - sphereCenter.GetZ());
	return (diffVec.SquaredLength() <= (radius * radius));
}

NiPoint3 NiPoint3::ClosestPointOnLine(const NiPoint3& a, const NiPoint3& b, const NiPoint3& p) {
	if (a == b) return a;

	const auto pa = p - a;
	const auto ab = b - a;

	const auto t = pa.DotProduct(ab) / ab.SquaredLength();

	if (t <= 0.0f) return a;

	if (t >= 1.0f) return b;

	return a + ab * t;
}

float NiPoint3::Angle(const NiPoint3& a, const NiPoint3& b) {
	const auto dot = a.DotProduct(b);
	const auto lenA = a.SquaredLength();
	const auto lenB = a.SquaredLength();
	return acos(dot / sqrt(lenA * lenB));
}

float NiPoint3::Distance(const NiPoint3& a, const NiPoint3& b) {
	const auto dx = a.x - b.x;
	const auto dy = a.y - b.y;
	const auto dz = a.z - b.z;

	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float NiPoint3::DistanceSquared(const NiPoint3& a, const NiPoint3& b) {
	const auto dx = a.x - b.x;
	const auto dy = a.y - b.y;
	const auto dz = a.z - b.z;

	return dx * dx + dy * dy + dz * dz;
}

NiPoint3 NiPoint3::MoveTowards(const NiPoint3& current, const NiPoint3& target, float maxDistanceDelta) {
	float dx = target.x - current.x;
	float dy = target.y - current.y;
	float dz = target.z - current.z;
	float lengthSquared = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
	if ((double)lengthSquared == 0.0 || (double)maxDistanceDelta >= 0.0 && (double)lengthSquared <= (double)maxDistanceDelta * (double)maxDistanceDelta)
		return target;
	float length = (float)std::sqrt((double)lengthSquared);
	return NiPoint3(current.x + dx / length * maxDistanceDelta, current.y + dy / length * maxDistanceDelta, current.z + dz / length * maxDistanceDelta);
}

//This code is yoinked from the MS XNA code, so it should be right, even if it's horrible.
NiPoint3 NiPoint3::RotateByQuaternion(const NiQuaternion& rotation) {
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
