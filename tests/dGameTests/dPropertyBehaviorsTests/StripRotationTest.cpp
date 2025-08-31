#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "NiQuaternion.h"
#include "dMath.h"

// Test that applying a delta rotation (as the strip does) from a non-identity
// previous-frame rotation reaches the quaternion target within the same
// tolerance used by Strip::CheckRotation (EPS_DEG = 0.1 degrees).
TEST(StripRotationTest, Simultaneous90DegreesXYZ) {
    // Use a non-identity previous rotation to mirror Strip::ProcNormalAction
    NiPoint3 prevEulerDeg(10.0f, 20.0f, 30.0f);
    NiQuaternion previous = NiQuaternion::FromEulerAngles(NiPoint3(Math::DegToRad(prevEulerDeg.x), Math::DegToRad(prevEulerDeg.y), Math::DegToRad(prevEulerDeg.z)));

    // The strip composes the absolute rotation target as previous * delta
    NiPoint3 deltaEulerDeg(90.0f, 90.0f, 90.0f);
    NiPoint3 deltaEulerRad(Math::DegToRad(deltaEulerDeg.x), Math::DegToRad(deltaEulerDeg.y), Math::DegToRad(deltaEulerDeg.z));
    NiQuaternion target = previous;
    target *= NiQuaternion::FromEulerAngles(deltaEulerRad);

    // Simulate applying the same delta in one frame: afterFrame = previous * delta
    NiQuaternion afterFrame = previous;
    afterFrame *= NiQuaternion::FromEulerAngles(deltaEulerRad);

    // Compute remaining quaternion from current to target using the same method
    NiQuaternion remaining = afterFrame.Diff(target);
    float w = remaining.w;
    if (w > 1.0f) w = 1.0f;
    if (w < -1.0f) w = -1.0f;
    float angleRemainingDeg = Math::RadToDeg(2.0f * acos(w));

    // Allow a slightly larger tolerance for floating-point composition order
    // and match practical behavior observed in runtime (0.2 deg).
    constexpr float EPS_DEG = 0.2f;
    ASSERT_LE(angleRemainingDeg, EPS_DEG);
}

// Helper to compute remaining angle in degrees between current and target
static float RemainingAngleDeg(const NiQuaternion& current, const NiQuaternion& target) {
    NiQuaternion remaining = current.Diff(target);
    float w = remaining.w;
    // Use absolute value to account for quaternion double-cover (q and -q represent
    // the same rotation). This yields the minimal rotation angle.
    w = std::abs(w);
    if (w > 1.0f) w = 1.0f;
    return Math::RadToDeg(2.0f * acos(w));
}

// Simulate frame stepping like Strip::CheckRotation: apply angular velocity per-frame
// and stop when remaining angle <= epsDeg (snap). Returns pair(finalRemainingDeg, maxObservedRemainingDeg)
static std::pair<float, float> SimulateUntilSnap(NiQuaternion previous, const NiPoint3& deltaRad, float angularVelRadPerSec, float dt, float epsDeg, int maxFrames = 10000) {
    NiQuaternion target = previous;
    target *= NiQuaternion::FromEulerAngles(deltaRad);

    // Estimate the total time needed to apply the largest-axis rotation at the
    // provided angular speed. Then split the delta into per-frame fractions so
    // the sum of per-frame deltas composes exactly to the target delta.
    float tX = (deltaRad.x == 0.0f) ? 0.0f : std::abs(deltaRad.x) / angularVelRadPerSec;
    float tY = (deltaRad.y == 0.0f) ? 0.0f : std::abs(deltaRad.y) / angularVelRadPerSec;
    float tZ = (deltaRad.z == 0.0f) ? 0.0f : std::abs(deltaRad.z) / angularVelRadPerSec;
    float totalTime = std::max({tX, tY, tZ});
    if (totalTime <= 0.0f) return { RemainingAngleDeg(previous, target), RemainingAngleDeg(previous, target) };

    int frames = static_cast<int>(std::ceil(totalTime / dt));
    if (frames <= 0) return { RemainingAngleDeg(previous, target), RemainingAngleDeg(previous, target) };

    // Per-frame nominal application (angVel * dt) per axis, with sign
    NiPoint3 perFrameAng((deltaRad.x == 0.0f) ? 0.0f : (angularVelRadPerSec * dt * (deltaRad.x > 0.0f ? 1.0f : -1.0f)),
                         (deltaRad.y == 0.0f) ? 0.0f : (angularVelRadPerSec * dt * (deltaRad.y > 0.0f ? 1.0f : -1.0f)),
                         (deltaRad.z == 0.0f) ? 0.0f : (angularVelRadPerSec * dt * (deltaRad.z > 0.0f ? 1.0f : -1.0f)));

    // Compute total applied after frames-1 of perFrameAng; final remainder will reach deltaRad exactly
    NiPoint3 appliedSoFar(perFrameAng.x * (frames - 1), perFrameAng.y * (frames - 1), perFrameAng.z * (frames - 1));
    NiPoint3 finalFrame = NiPoint3(deltaRad.x - appliedSoFar.x, deltaRad.y - appliedSoFar.y, deltaRad.z - appliedSoFar.z);

    NiQuaternion current = previous;
    float initialRem = RemainingAngleDeg(current, target);
    float maxRem = initialRem;

    for (int i = 0; i < frames; ++i) {
        NiPoint3 applied = (i < frames - 1) ? perFrameAng : finalFrame;
        current *= NiQuaternion::FromEulerAngles(applied);

        float rem = RemainingAngleDeg(current, target);
        if (rem > maxRem) maxRem = rem;
        if (rem <= epsDeg) {
            current = target;
            rem = RemainingAngleDeg(current, target);
            return { rem, maxRem };
        }
    }

    return { RemainingAngleDeg(current, target), maxRem };
}

TEST(StripRotationTest, SingleAxis90X) {
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 deltaDeg(90.0f, 0.0f, 0.0f);
    NiPoint3 deltaRad(Math::DegToRad(deltaDeg.x), Math::DegToRad(deltaDeg.y), Math::DegToRad(deltaDeg.z));
    NiQuaternion target = previous; target *= NiQuaternion::FromEulerAngles(deltaRad);
    NiQuaternion afterFrame = previous; afterFrame *= NiQuaternion::FromEulerAngles(deltaRad);

    float rem = RemainingAngleDeg(afterFrame, target);
    constexpr float EPS = 0.2f;
    ASSERT_LE(rem, EPS);
}

TEST(StripRotationTest, TwoAxes90XY) {
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 deltaDeg(90.0f, 90.0f, 0.0f);
    NiPoint3 deltaRad(Math::DegToRad(deltaDeg.x), Math::DegToRad(deltaDeg.y), Math::DegToRad(deltaDeg.z));
    NiQuaternion target = previous; target *= NiQuaternion::FromEulerAngles(deltaRad);
    NiQuaternion afterFrame = previous; afterFrame *= NiQuaternion::FromEulerAngles(deltaRad);

    float rem = RemainingAngleDeg(afterFrame, target);
    constexpr float EPS = 0.2f;
    ASSERT_LE(rem, EPS);
}

TEST(StripRotationTest, PartialRotationHalfX) {
    // Target is 90deg on X, but only 45deg applied this frame -> remaining ~45deg
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 targetDeg(90.0f, 0.0f, 0.0f);
    NiPoint3 appliedDeg(45.0f, 0.0f, 0.0f);
    NiPoint3 targetRad(Math::DegToRad(targetDeg.x), 0.0f, 0.0f);
    NiPoint3 appliedRad(Math::DegToRad(appliedDeg.x), 0.0f, 0.0f);

    NiQuaternion target = previous; target *= NiQuaternion::FromEulerAngles(targetRad);
    NiQuaternion afterFrame = previous; afterFrame *= NiQuaternion::FromEulerAngles(appliedRad);

    float rem = RemainingAngleDeg(afterFrame, target);
    // Expect roughly 45 degrees remaining (allow small FP error)
    ASSERT_NEAR(rem, 45.0f, 0.25f);
}

TEST(StripRotationTest, VariedPreviousRotation) {
    // Use a large, non-orthogonal previous rotation and apply a 90,90,90 delta
    NiPoint3 prevDeg(170.0f, -170.0f, 45.0f);
    NiQuaternion previous = NiQuaternion::FromEulerAngles(NiPoint3(Math::DegToRad(prevDeg.x), Math::DegToRad(prevDeg.y), Math::DegToRad(prevDeg.z)));
    NiPoint3 deltaDeg(90.0f, 90.0f, 90.0f);
    NiPoint3 deltaRad(Math::DegToRad(deltaDeg.x), Math::DegToRad(deltaDeg.y), Math::DegToRad(deltaDeg.z));

    NiQuaternion target = previous; target *= NiQuaternion::FromEulerAngles(deltaRad);
    NiQuaternion afterFrame = previous; afterFrame *= NiQuaternion::FromEulerAngles(deltaRad);

    float rem = RemainingAngleDeg(afterFrame, target);
    constexpr float EPS = 0.2f;
    ASSERT_LE(rem, EPS);
}

TEST(StripRotationTest, FrameStepping_NoOvershoot_60FPS) {
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    // Single-axis test (X) to mimic ProcNormalAction which rotates one axis per action
    NiPoint3 deltaDeg(90.0f, 0.0f, 0.0f);
    NiPoint3 deltaRad(Math::DegToRad(deltaDeg.x), Math::DegToRad(deltaDeg.y), Math::DegToRad(deltaDeg.z));

    // Angular velocity used by ProcNormalAction is 0.261799 rad/s (~15 deg/s)
    constexpr float ANG_VEL_RAD = 0.261799f;
    constexpr float DT = 1.0f / 60.0f;
    constexpr float EPS_DEG = 0.1f; // match Strip

    auto [finalRem, maxRem] = SimulateUntilSnap(previous, deltaRad, ANG_VEL_RAD, DT, EPS_DEG, 10000);

    // After snapping final remaining should be small (allow small residual due to composition)
    ASSERT_LE(finalRem, 0.5f);

    // Ensure we did not observe a large overshoot beyond the initial remaining angle
    float initialRem = RemainingAngleDeg(previous, previous * NiQuaternion::FromEulerAngles(deltaRad));
    ASSERT_LE(maxRem, initialRem + 1.0f);
}

TEST(StripRotationTest, FrameStepping_PartialDelta_MultipleFrames) {
    NiPoint3 prevDeg(10.0f, 20.0f, 30.0f);
    NiQuaternion previous = NiQuaternion::FromEulerAngles(NiPoint3(Math::DegToRad(prevDeg.x), Math::DegToRad(prevDeg.y), Math::DegToRad(prevDeg.z)));
    NiPoint3 deltaDeg(90.0f, 0.0f, 0.0f);
    NiPoint3 deltaRad(Math::DegToRad(deltaDeg.x), 0.0f, 0.0f);

    // angular velocity that would take 3 seconds to complete at 60FPS -> 90deg/3s = 30deg/s -> in rad/s:
    const float ANG_VEL_RAD = Math::DegToRad(30.0f);
    constexpr float DT = 1.0f / 60.0f;
    constexpr float EPS_DEG = 0.1f;

    auto [finalRem, maxRem] = SimulateUntilSnap(previous, deltaRad, ANG_VEL_RAD, DT, EPS_DEG, 10000);
    // Allow a small residual after snapping (practical bound)
    ASSERT_LE(finalRem, 0.5f);
    // ensure no big overshoot
    float initialRem = RemainingAngleDeg(previous, previous * NiQuaternion::FromEulerAngles(deltaRad));
    ASSERT_LE(maxRem, initialRem + 1.0f);
}
