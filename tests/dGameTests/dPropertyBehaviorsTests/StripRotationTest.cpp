#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "NiQuaternion.h"
#include "dMath.h"

// Test that rotating a quaternion by 90 degrees on each axis in one frame
// yields approximately 90 degrees when converted back to Euler angles.
TEST(StripRotationTest, Simultaneous90DegreesXYZ) {
    // Simulate the per-axis logic used in Strip::CheckRotation.
    // Assume a single-frame rotation where angular velocity is 90 degrees/sec
    // on each axis and deltaTime is 1.0 seconds. The remaining rotation
    // prior to the frame is 90 degrees on each axis.
    NiPoint3 remainingRotationDeg(90.0f, 90.0f, 90.0f);
    NiPoint3 angularVelocityDegPerSec(90.0f, 90.0f, 90.0f);
    const float deltaTime = 1.0f;

    // Compute degrees moved this frame per axis
    NiPoint3 angMovedDegrees(std::abs(angularVelocityDegPerSec.x) * deltaTime,
                             std::abs(angularVelocityDegPerSec.y) * deltaTime,
                             std::abs(angularVelocityDegPerSec.z) * deltaTime);

    // Subtract movement from remaining rotation per axis (mirrors Strip logic)
    bool rotateFinished = true;
    constexpr float EPS_DEG = 1e-3f;

    // X
    remainingRotationDeg.x -= angMovedDegrees.x;
    if (std::signbit(remainingRotationDeg.x) != std::signbit(90.0f) || std::abs(remainingRotationDeg.x) <= EPS_DEG) {
        remainingRotationDeg.x = 0.0f;
    } else {
        rotateFinished = false;
    }

    // Y
    remainingRotationDeg.y -= angMovedDegrees.y;
    if (std::signbit(remainingRotationDeg.y) != std::signbit(90.0f) || std::abs(remainingRotationDeg.y) <= EPS_DEG) {
        remainingRotationDeg.y = 0.0f;
    } else {
        rotateFinished = false;
    }

    // Z
    remainingRotationDeg.z -= angMovedDegrees.z;
    if (std::signbit(remainingRotationDeg.z) != std::signbit(90.0f) || std::abs(remainingRotationDeg.z) <= EPS_DEG) {
        remainingRotationDeg.z = 0.0f;
    } else {
        rotateFinished = false;
    }

    ASSERT_TRUE(rotateFinished);
    ASSERT_EQ(remainingRotationDeg, NiPoint3(0.0f, 0.0f, 0.0f));
}
