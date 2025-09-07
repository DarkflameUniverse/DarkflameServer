#ifndef NIQUATERNION_H
#define NIQUATERNION_H

// Custom Classes
#include "NiPoint3.h"

#define GLM_FORCE_QUAT_DATA_WXYZ

#include <glm/ext/quaternion_float.hpp>

using Quaternion = glm::quat;
using NiQuaternion = Quaternion;

namespace QuatUtils {
	constexpr NiQuaternion IDENTITY = glm::identity<NiQuaternion>();
	Vector3 Forward(const NiQuaternion& quat);
	Vector3 Up(const NiQuaternion& quat);
	Vector3 Right(const NiQuaternion& quat);
	NiQuaternion LookAt(const NiPoint3& from, const NiPoint3& to);
	NiQuaternion LookAtUnlocked(const NiPoint3& from, const NiPoint3& to);
	Vector3 Euler(const NiQuaternion& quat);
	NiQuaternion AxisAngle(const Vector3& axis, float angle);
	NiQuaternion FromEuler(const NiPoint3& eulerAngles);
	constexpr float PI_OVER_180 = glm::pi<float>() / 180.0f;
};

#endif // !NIQUATERNION_H
