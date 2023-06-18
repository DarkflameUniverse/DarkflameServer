#include "lCommonTypes.h"

#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LuaScript.h"

void lCommonTypes::RegisterClass(LuaScript* script)
{
    sol::state& lua = script->GetState();

    lua.new_usertype<NiPoint3>(
        "Vector3",
        sol::constructors<NiPoint3(), NiPoint3(float, float, float)>(),
        "x", &NiPoint3::x,
        "y", &NiPoint3::y,
        "z", &NiPoint3::z,
        "dot", &NiPoint3::DotProduct,
        "cross", &NiPoint3::CrossProduct,
        "length", &NiPoint3::Length,
        "squaredLength", &NiPoint3::SquaredLength,
        "unitize", &NiPoint3::Unitize,
        "normalize", &NiPoint3::Unitize,
        "distance", &NiPoint3::Distance,
        "distanceSquared", &NiPoint3::DistanceSquared,
        "getX", &NiPoint3::GetX,
        "getY", &NiPoint3::GetY,
        "getZ", &NiPoint3::GetZ,
        "setX", &NiPoint3::SetX,
        "setY", &NiPoint3::SetY,
        "setZ", &NiPoint3::SetZ,
        sol::meta_function::to_string, [](const NiPoint3& vector) {
            return "Vector3(" + std::to_string(vector.x) + ", " + std::to_string(vector.y) + ", " + std::to_string(vector.z) + ")";
        },
        sol::meta_function::addition, [](const NiPoint3& lhs, const NiPoint3& rhs) {
            return lhs + rhs;
        },
        sol::meta_function::subtraction, [](const NiPoint3& lhs, const NiPoint3& rhs) {
            return lhs - rhs;
        },
        sol::meta_function::multiplication, [](const NiPoint3& lhs, const float& rhs) {
            return lhs * rhs;
        },
        sol::meta_function::division, [](const NiPoint3& lhs, const float& rhs) {
            return lhs / rhs;
        },
        sol::meta_function::unary_minus, [](const NiPoint3& lhs) {
            return lhs * -1.0f;
        },
        sol::meta_function::equal_to, [](const NiPoint3& lhs, const NiPoint3& rhs) {
            return lhs == rhs;
        }
    );

    lua.new_usertype<NiQuaternion>(
        "Quaternion",
        sol::constructors<NiQuaternion(), NiQuaternion(float, float, float, float)>(),
        "x", &NiQuaternion::x,
        "y", &NiQuaternion::y,
        "z", &NiQuaternion::z,
        "w", &NiQuaternion::w,
        "getForwardVector", &NiQuaternion::GetForwardVector,
        "getUpVector", &NiQuaternion::GetUpVector,
        "getRightVector", &NiQuaternion::GetRightVector,
        "getEulerAngles", &NiQuaternion::GetEulerAngles,
        "getX", &NiQuaternion::GetX,
        "getY", &NiQuaternion::GetY,
        "getZ", &NiQuaternion::GetZ,
        "getW", &NiQuaternion::GetW,
        "setX", &NiQuaternion::SetX,
        "setY", &NiQuaternion::SetY,
        "setZ", &NiQuaternion::SetZ,
        "setW", &NiQuaternion::SetW,
        sol::meta_function::to_string, [](const NiQuaternion& quaternion) {
            return "Quaternion(" + std::to_string(quaternion.x) + ", " + std::to_string(quaternion.y) + ", " + std::to_string(quaternion.z) + ", " + std::to_string(quaternion.w) + ")";
        },
        sol::meta_function::equal_to, [](const NiQuaternion& lhs, const NiQuaternion& rhs) {
            return lhs == rhs;
        }
    );
}
