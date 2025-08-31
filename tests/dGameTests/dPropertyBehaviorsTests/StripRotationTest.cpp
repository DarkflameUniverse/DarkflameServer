#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "NiQuaternion.h"
#include "dMath.h"

// Test that rotating a quaternion by 90 degrees on each axis in one frame
// yields approximately 90 degrees when converted back to Euler angles.
TEST(StripRotationTest, Simultaneous90DegreesXYZ) {
    // Use quaternion math to verify a single-frame rotation of 90deg on each axis
    // reaches the composed target. Start rotation is identity.
    NiQuaternion start = NiQuaternionConstant::IDENTITY;
    NiPoint3 targetEulerRad(Math::DegToRad(90.0f), Math::DegToRad(90.0f), Math::DegToRad(90.0f));
    NiQuaternion target = NiQuaternion::FromEulerAngles(targetEulerRad);

    // Simulate applying angular velocity of 90deg/sec on each axis for 1 second
    NiPoint3 appliedEulerRad = targetEulerRad; // angularVel * deltaTime
    NiQuaternion afterFrame = start;
    afterFrame *= NiQuaternion::FromEulerAngles(appliedEulerRad);

    // Remaining quaternion from current to target should be identity (or near it)
    NiQuaternion remaining = afterFrame.Diff(target);
    float w = remaining.w;
    if (w > 1.0f) w = 1.0f;
    if (w < -1.0f) w = -1.0f;
    float angleRemainingDeg = Math::RadToDeg(2.0f * acos(w));

    // Allow a small residual due to floating point and composition order
    ASSERT_LE(angleRemainingDeg, 0.2f);
}
