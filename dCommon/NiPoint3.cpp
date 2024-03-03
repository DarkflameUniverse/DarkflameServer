#include "NiPoint3.h"

// C++
#include <cmath>

// MARK: Member Functions

//! Gets the length of the vector
float NiPoint3::Length() const {
	return std::sqrt(x * x + y * y + z * z);
}

//! Unitize the vector
NiPoint3 NiPoint3::Unitize() const {
	float length = this->Length();

	return length != 0 ? *this / length : NiPoint3Constant::ZERO;
}

// MARK: Helper Functions

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

NiPoint3 NiPoint3::MoveTowards(const NiPoint3& current, const NiPoint3& target, const float maxDistanceDelta) {
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
