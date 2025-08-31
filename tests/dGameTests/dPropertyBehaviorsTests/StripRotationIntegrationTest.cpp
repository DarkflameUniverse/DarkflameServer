#define UNIT_TEST
#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "ModelComponent.h"
#include "SimplePhysicsComponent.h"
#include "Strip.h"
#include "NiQuaternion.h"
#include "NiPoint3.h"
#include "dMath.h"

using namespace std::literals;

static float RemainingAngleDeg(const NiQuaternion& cur, const NiQuaternion& target) {
    auto rem = cur.Diff(target);
    float w = rem.w;
    if (w < 0.0f) w = -w; // minimal quaternion
    if (w > 1.0f) w = 1.0f;
    return 2.0f * std::acos(w) * (180.0f / 3.14159265358979323846f);
}

// Test accessor must be global to match friend declaration in Strip.h
#ifdef UNIT_TEST
struct StripTestAccessor { static void InitRotation(Strip& s, const NiQuaternion& prev, const NiQuaternion& targ) {
    s.m_IsRotating = true;
    s.m_PreviousFrameRotation = prev;
    s.m_RotationTarget = targ;
}};
#else
struct StripTestAccessor { static void InitRotation(Strip&, const NiQuaternion&, const NiQuaternion&) {} };
#endif

// Integration-style harness: instantiate Entity+Components, set up a Strip rotation, step SimplePhysicsComponent and call Strip::CheckRotation
TEST_F(GameDependenciesTest, SimulateStripRotationNoOvershoot) {
    // Inline a lightweight dependency setup here to avoid loading CDClient defaults which
    // attempt database access in this unit test environment.
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    info.scale = 1.0f;
    info.spawner = nullptr;
    info.lot = 999;
    Game::logger = new Logger("./testing.log", true, true);
    Game::server = new dServerMock();
    Game::config = new dConfig("worldconfig.ini");
    Game::entityManager = new EntityManager();
    Game::zoneManager = new dZoneManager();
    Database::_setDatabase(new TestSQLDatabase());
    // Ensure CD client defaults are present so Entity initialization doesn't hit the DB
    CDClientManager::LoadValuesFromDefaults();
    Game::zoneManager->LoadZone(LWOZONEID(1, 0, 0));
    // Build a minimal EntityInfo and Entity
    EntityInfo info;
    info.lot = 0;
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    Entity* entity = Game::entityManager->CreateEntity(info, nullptr, nullptr);

    // Attach ModelComponent and SimplePhysicsComponent
    auto* model = entity->AddComponent<ModelComponent>();
    auto* phys = entity->AddComponent<SimplePhysicsComponent>(0);

    // Prepare a Strip and configure it as if an action started: previous rotation and a 90deg XYZ delta target
    Strip strip;
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 deltaDeg{90.0f, 90.0f, 90.0f};
    NiPoint3 deltaRad = NiPoint3{deltaDeg.x, deltaDeg.y, deltaDeg.z} * (3.14159265f / 180.0f);
    NiQuaternion deltaQ = NiQuaternion::FromEulerAngles(deltaRad);
    NiQuaternion target = previous * deltaQ;

    StripTestAccessor::InitRotation(strip, previous, target);

    // Set entity rotation to previous
    entity->SetRotation(previous);

    // Simulate applying the delta in one frame by setting angular velocity so that Update will rotate the entity by deltaRad
    // SimplePhysicsComponent applies rotation as FromEulerAngles(angularVelocity * dt)
    float dt = 1.0f / 60.0f;
    NiPoint3 requiredAngVel = NiPoint3{deltaRad.x / dt, deltaRad.y / dt, deltaRad.z / dt};
    phys->SetAngularVelocity(requiredAngVel);

    // Step physics once
    phys->Update(dt);

    // Now call Strip::CheckRotation which should observe the entity's rotation and snap because remaining <= EPS
    bool finished = strip.CheckRotation(dt, *model);
    EXPECT_TRUE(finished);

    // Verify final rotation was snapped to exactly target
    auto finalRot = entity->GetRotation();
    float rem = RemainingAngleDeg(finalRot, target);
    EXPECT_LE(rem, 0.2f);

    TearDownDependencies();
}

// Multi-frame rotation: apply a 90deg X rotation over many frames and ensure no overshoot
TEST_F(GameDependenciesTest, MultiFrameRotation_NoOvershoot) {
    // Inline setup as above (avoid CDClientManager DB access)
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    info.scale = 1.0f;
    info.spawner = nullptr;
    info.lot = 999;
    Game::logger = new Logger("./testing.log", true, true);
    Game::server = new dServerMock();
    Game::config = new dConfig("worldconfig.ini");
    Game::entityManager = new EntityManager();
    Game::zoneManager = new dZoneManager();
    Database::_setDatabase(new TestSQLDatabase());
    CDClientManager::LoadValuesFromDefaults();
    Game::zoneManager->LoadZone(LWOZONEID(1, 0, 0));

    EntityInfo info;
    info.lot = 0;
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    Entity* entity = Game::entityManager->CreateEntity(info, nullptr, nullptr);

    auto* model = entity->AddComponent<ModelComponent>();
    auto* phys = entity->AddComponent<SimplePhysicsComponent>(0);

    Strip strip;
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 deltaDeg{90.0f, 0.0f, 0.0f};
    NiPoint3 deltaRad = NiPoint3{deltaDeg.x, deltaDeg.y, deltaDeg.z} * (3.14159265f / 180.0f);
    NiQuaternion target = previous * NiQuaternion::FromEulerAngles(deltaRad);

    StripTestAccessor::InitRotation(strip, previous, target);
    entity->SetRotation(previous);

    // Use a moderate angular velocity: 30 deg/s -> 0.5235987756 rad/s
    const float angVelRad = Math::DegToRad(30.0f);
    const float dt = 1.0f / 60.0f;

    // Set angular velocity on physics component (rad/s)
    phys->SetAngularVelocity(NiPoint3{angVelRad, 0.0f, 0.0f});

    float initialRem = RemainingAngleDeg(previous, target);
    float maxRem = initialRem;
    const int maxFrames = 10000;
    bool finished = false;

    for (int i = 0; i < maxFrames; ++i) {
    phys->Update(dt);
    float rem = RemainingAngleDeg(entity->GetRotation(), target);
        if (rem > maxRem) maxRem = rem;
        if (strip.CheckRotation(dt, *model)) { finished = true; break; }
    }

    EXPECT_TRUE(finished);
    float finalRem = RemainingAngleDeg(entity->GetRotation(), target);
    EXPECT_LE(finalRem, 0.2f);
    EXPECT_LE(maxRem, initialRem + 1.0f);

    TearDownDependencies();
}

// Multi-axis multi-frame rotation: apply 90deg on X/Y/Z over several frames
TEST_F(GameDependenciesTest, MultiFrame_MultiAxis_NoOvershoot) {
    // Inline setup as above (avoid CDClientManager DB access)
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    info.scale = 1.0f;
    info.spawner = nullptr;
    info.lot = 999;
    Game::logger = new Logger("./testing.log", true, true);
    Game::server = new dServerMock();
    Game::config = new dConfig("worldconfig.ini");
    Game::entityManager = new EntityManager();
    Game::zoneManager = new dZoneManager();
    Database::_setDatabase(new TestSQLDatabase());
    CDClientManager::LoadValuesFromDefaults();
    Game::zoneManager->LoadZone(LWOZONEID(1, 0, 0));

    EntityInfo info;
    info.lot = 0;
    info.pos = NiPoint3Constant::ZERO;
    info.rot = NiQuaternionConstant::IDENTITY;
    Entity* entity = Game::entityManager->CreateEntity(info, nullptr, nullptr);

    auto* model = entity->AddComponent<ModelComponent>();
    auto* phys = entity->AddComponent<SimplePhysicsComponent>(0);

    Strip strip;
    NiQuaternion previous = NiQuaternionConstant::IDENTITY;
    NiPoint3 deltaDeg{90.0f, 90.0f, 90.0f};
    NiPoint3 deltaRad = NiPoint3{deltaDeg.x, deltaDeg.y, deltaDeg.z} * (3.14159265f / 180.0f);
    NiQuaternion target = previous * NiQuaternion::FromEulerAngles(deltaRad);

    StripTestAccessor::InitRotation(strip, previous, target);
    entity->SetRotation(previous);

    // Perform the multi-axis rotation as three sequential single-axis actions (X, then Y, then Z)
    const float angVelRad = Math::DegToRad(15.0f);
    const float dt = 1.0f / 60.0f;

    float initialRem = RemainingAngleDeg(previous, target);
    float maxRem = initialRem;
    const int maxFramesPerAxis = 10000;

    NiQuaternion currentPrev = previous;
    bool allFinished = true;

    // helper to run one axis rotation
    auto runAxis = [&](const NiPoint3& axisVel, const NiQuaternion& axisTarget) -> bool {
        phys->SetAngularVelocity(axisVel);
        for (int i = 0; i < maxFramesPerAxis; ++i) {
        phys->Update(dt);
        float rem = RemainingAngleDeg(entity->GetRotation(), axisTarget);
            if (rem > maxRem) maxRem = rem;
            if (strip.CheckRotation(dt, *model)) return true;
        }
        return false;
    };

    // X axis (90 deg)
    NiQuaternion targetX = currentPrev * NiQuaternion::FromEulerAngles(NiPoint3{Math::DegToRad(90.0f), 0.0f, 0.0f});
    StripTestAccessor::InitRotation(strip, currentPrev, targetX);
    if (!runAxis(NiPoint3{angVelRad, 0.0f, 0.0f}, targetX)) allFinished = false;
    currentPrev = entity->GetRotation();

    // Y axis (90 deg)
    NiQuaternion targetY = currentPrev * NiQuaternion::FromEulerAngles(NiPoint3{0.0f, Math::DegToRad(90.0f), 0.0f});
    StripTestAccessor::InitRotation(strip, currentPrev, targetY);
    if (!runAxis(NiPoint3{0.0f, angVelRad, 0.0f}, targetY)) allFinished = false;
    currentPrev = entity->GetRotation();

    // Z axis (90 deg)
    NiQuaternion targetZ = currentPrev * NiQuaternion::FromEulerAngles(NiPoint3{0.0f, 0.0f, Math::DegToRad(90.0f)});
    StripTestAccessor::InitRotation(strip, currentPrev, targetZ);
    if (!runAxis(NiPoint3{0.0f, 0.0f, angVelRad}, targetZ)) allFinished = false;

    EXPECT_TRUE(allFinished);
    float finalRem = RemainingAngleDeg(entity->GetRotation(), targetZ);
    EXPECT_LE(finalRem, 0.2f);
    EXPECT_LE(maxRem, initialRem + 2.0f); // multi-axis sequential should still be bounded

    TearDownDependencies();
}
