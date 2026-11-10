/*
 * Darkflame Universe
 * Copyright 2019
 */

#include "MovingPlatformComponent.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "SimplePhysicsComponent.h"
#include "Zone.h"
#include "CDClientDatabase.h"
#include "MovementMessages.h"

#include <glm/gtc/quaternion.hpp>
#include "Entity.h"
#include "ObjectLoadState.h"
#include "MovementAIComponent.h"
#include "QuickBuildComponent.h"
#include "PhantomPhysicsComponent.h"
#include "eQuickBuildState.h"

void PlatformSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write<bool>(true);

	outBitStream.Write<uint32_t>(mState);
	outBitStream.Write<int32_t>(mDesiredWaypointIndex);
	outBitStream.Write(mShouldStopAtDesiredWaypoint);
	outBitStream.Write(mInReverse);

	outBitStream.Write<float_t>(mPercentBetweenPoints);

	outBitStream.Write<float_t>(mPosition.x);
	outBitStream.Write<float_t>(mPosition.y);
	outBitStream.Write<float_t>(mPosition.z);

	outBitStream.Write<uint32_t>(mCurrentWaypointIndex);
	outBitStream.Write<uint32_t>(mNextWaypointIndex);

	outBitStream.Write<float_t>(mIdleTimeElapsed);
	outBitStream.Write<float_t>(mMoveTimeElapsed);
}

//------------- SimpleMoverSubComponent below --------------

// LWOPlatformSimpleMover::Deserialize: a dirty bit, then whether there is a starting point with its position and
// rotation (w, x, y, z); a dirty bit, then the state, the current waypoint and whether it is reversing
void SimpleMoverSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(bIsInitialUpdate || mDirtyStartingPoint);
	if (bIsInitialUpdate || mDirtyStartingPoint) {
		outBitStream.Write1();
		outBitStream.Write(mStartPosition.x);
		outBitStream.Write(mStartPosition.y);
		outBitStream.Write(mStartPosition.z);
		outBitStream.Write(mStartRotation.w);
		outBitStream.Write(mStartRotation.x);
		outBitStream.Write(mStartRotation.y);
		outBitStream.Write(mStartRotation.z);
		if (!bIsInitialUpdate) mDirtyStartingPoint = false;
	}

	outBitStream.Write(bIsInitialUpdate || mDirtyState);
	if (bIsInitialUpdate || mDirtyState) {
		outBitStream.Write<uint32_t>(mState);
		outBitStream.Write<int32_t>(static_cast<int32_t>(mCurrentWaypointIndex));
		outBitStream.Write(mInReverse);
		if (!bIsInitialUpdate) mDirtyState = false;
	}
}

NiPoint3 SimpleMoverSubComponent::GetWaypointPosition(const int32_t index) const {
	if (index <= 0) return mStartPosition;
	const glm::vec3 moved = mStartRotation * glm::vec3(mMove.x, mMove.y, mMove.z);
	return mStartPosition + NiPoint3(moved.x, moved.y, moved.z);
}

std::vector<eMoverSubComponentType> ChooseSubComponentTypes(const int32_t componentID, const Entity& entity) {
	const bool setRotater = entity.HasVar(u"platformIsRotater");
	const bool setMover = entity.HasVar(u"platformIsMover");
	const bool setSimpleMover = entity.HasVar(u"platformIsSimpleMover");

	if (!setRotater && !setMover && !setSimpleMover) {
		return { componentID < 1 ? eMoverSubComponentType::mover : eMoverSubComponentType::simpleMover };
	}

	std::vector<eMoverSubComponentType> types;
	if (setMover && entity.GetVar<bool>(u"platformIsMover")) types.push_back(eMoverSubComponentType::mover);
	if (setSimpleMover && entity.GetVar<bool>(u"platformIsSimpleMover")) types.push_back(eMoverSubComponentType::simpleMover);
	if (setRotater && entity.GetVar<bool>(u"platformIsRotater")) types.push_back(eMoverSubComponentType::rotater);
	return types;
}

eMoverSubComponentType ChooseMoverSubComponentType(const int32_t componentID, const Entity& entity) {
	const auto types = ChooseSubComponentTypes(componentID, entity);
	return types.empty() ? eMoverSubComponentType::none : types.front();
}

//------------- MovingPlatformComponent below --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName) : Component(parent, componentID) {
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_NoAutoStart = false;
}

// The subcomponents (LWOMovingPlatformComponent::SetupPlatform: one per type, in the client's order), and a simple
// mover's MovingPlatforms row
void MovingPlatformComponent::LoadDataFromTemplate() {
	for (const auto type : ChooseSubComponentTypes(m_ComponentID, *m_Parent)) {
		if (type == eMoverSubComponentType::simpleMover) {
			auto simpleMover = std::make_unique<SimpleMoverSubComponent>();
			simpleMover->mStartPosition = m_Parent->GetDefaultPosition();
			simpleMover->mStartRotation = m_Parent->GetDefaultRotation();

			// LWOPlatformSimpleMover::LoadTemplateData: the MovingPlatforms row, unless the object sets dbonly false
			const bool dbOnly = m_Parent->GetVar<bool>(u"dbonly");
			if (m_ComponentID > 0 && (!m_Parent->HasVar(u"dbonly") || dbOnly)) {
				auto query = CDClientDatabase::CreatePreppedStmt(
					"SELECT platformMoveX, platformMoveY, platformMoveZ, platformMoveTime, platformStartAtEnd FROM MovingPlatforms WHERE id = ?;");
				query.bind(1, m_ComponentID);
				auto result = query.execQuery();
				if (!result.eof()) {
					simpleMover->mMove = NiPoint3(result.getFloatField(0), result.getFloatField(1), result.getFloatField(2));
					simpleMover->mMoveTime = result.getFloatField(3);
					simpleMover->mStartAtEnd = result.getIntField(4) != 0;
				} else {
					LOG("LOT %i couldn't find simple mover %i in MovingPlatforms", m_Parent->GetLOT(), m_ComponentID);
				}
			}
			m_SubComponents.push_back(std::move(simpleMover));
		} else if (type == eMoverSubComponentType::rotater) {
			m_SubComponents.push_back(std::make_unique<RotatorSubComponent>(m_Parent->GetDefaultPosition()));
		} else {
			m_SubComponents.push_back(std::make_unique<MoverSubComponent>(m_Parent->GetDefaultPosition()));
		}
	}
	m_MoverSubComponentType = m_SubComponents.empty() ? eMoverSubComponentType::none : m_SubComponents.front()->GetType();
}

// The level config: a simple mover's settings, the path and where and whether a mover starts
void MovingPlatformComponent::LoadConfigData() {
	for (const auto& subComponent : m_SubComponents) {
		if (subComponent->GetType() != eMoverSubComponentType::simpleMover) continue;
		auto* const simpleMover = static_cast<SimpleMoverSubComponent*>(subComponent.get());
		const bool dbOnly = m_Parent->GetVar<bool>(u"dbonly");

		// LWOPlatformSimpleMover::LoadConfigData: the object's settings over the table's, unless it is db only
		if (m_Parent->HasVar(u"attached_path_start")) simpleMover->mStartAtEnd = m_Parent->GetVar<int32_t>(u"attached_path_start") != 0;
		if (m_Parent->HasVar(u"platformStartAtEnd")) simpleMover->mStartAtEnd = m_Parent->GetVar<bool>(u"platformStartAtEnd");
		if (!dbOnly) {
			if (m_Parent->HasVar(u"platformMoveX")) simpleMover->mMove.x = m_Parent->GetVar<float>(u"platformMoveX");
			if (m_Parent->HasVar(u"platformMoveY")) simpleMover->mMove.y = m_Parent->GetVar<float>(u"platformMoveY");
			if (m_Parent->HasVar(u"platformMoveZ")) simpleMover->mMove.z = m_Parent->GetVar<float>(u"platformMoveZ");
			if (m_Parent->HasVar(u"platformMoveTime")) simpleMover->mMoveTime = m_Parent->GetVar<float>(u"platformMoveTime");
		}
		simpleMover->mCurrentWaypointIndex = simpleMover->mStartAtEnd ? 1 : 0;

		// As a mover on its path (LWOPlatform::ResumeTravelling 0x00c86b90 and SetWaitingTime 0x00bdc860): unless
		// startPathingOnLoad is false it waits at its waypoint (9), for the shortest wait as its waypoints have none.
		// Live's scripts for the NJ spinners, which are simple movers that only move when used, stop them when they
		// start up (L_NJ_SPINNER_SERVER: StopPathing), and the simple movers that wait for an event without such a script
		// have startPathingOnLoad 0.
		const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
		if (startOnLoad) {
			simpleMover->mState = ePlatformStateFlag::Waiting | ePlatformStateFlag::ReachedDesiredWaypoint;
			simpleMover->mTimeToWait = 0.1f;
			simpleMover->mWaitSet = true;
		}
	}

	const auto pathName = GeneralUtils::UTF16ToWTF8(m_PathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);

	if (m_Path == nullptr) {
		LOG("Path not found: %s", pathName.c_str());
	}

	if (IsMover()) {
		const auto start = m_Parent->HasVar(u"attached_path_start") ? m_Parent->GetVar<int32_t>(u"attached_path_start") : 0;
		m_StartWaypoint = start >= 0 && static_cast<size_t>(start) < m_Path->pathWaypoints.size() ? start : 0;
		SetupMoverPath();
	} else if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		const auto start = m_Parent->HasVar(u"attached_path_start") ? m_Parent->GetVar<int32_t>(u"attached_path_start") : 0;
		m_StartWaypoint = start >= 0 ? start : 0;
		SetupRotator();
	}

	// Live constructed every moving platform with simple physics as keyframed
	auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>();
	if (simplePhysics && !m_Parent->HasVar(u"motionType")) simplePhysics->SetPhysicsMotionState(SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);
}

// LWOPlatform::SetupPath 0x00c17d10: a mover is at its starting waypoint, stopped (12, as live constructed movers nothing
// had started). Unless startPathingOnLoad is false it then waits the waypoint's wait and leaves (live: 9).
void MovingPlatformComponent::SetupMoverPath() {
	auto* const mover = GetMoverSubComponent();
	mover->mTimeBased = m_Path->movingPlatform.timeBasedMovement != 0;
	mover->mInReverse = m_PathReverse;
	MoverProcessStateChange(m_StartWaypoint);
	const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
	if (startOnLoad) MoverResumeTravelling();
}

// LWOPlatformRotator: on its path it only turns (rotX/rotY/rotZ at rate; its HandleStoppedLogic 0x00bdc660 only keeps
// the angular velocity, so it never arrives anywhere), while travelling. Only one on a path runs
// (LWOMovingPlatformComponent::SetupPlatform 0x00ccea50), and the client reads a platform that is not at a waypoint
// from its path (LWOPlatform::Deserialize 0x00c870e0), so one without a path stays as it was made (12). The client's
// rotator starts turning when it is sent travelling (Deserialize: StartTravelling, ResumeTravelling, UpdateVelocities).
void MovingPlatformComponent::SetupRotator() {
	auto* const rotator = static_cast<RotatorSubComponent*>(GetMoverSubComponent());
	// LWOPlatformRotator::LoadConfigData 0x00c33e10
	constexpr float pi = 3.14159265358979323846f;
	if (m_Parent->HasVar(u"rotX")) rotator->mRotationX = m_Parent->GetVar<float>(u"rotX") * pi;
	if (m_Parent->HasVar(u"rotY")) rotator->mRotationY = m_Parent->GetVar<float>(u"rotY") * pi;
	if (m_Parent->HasVar(u"rotZ")) rotator->mRotationZ = m_Parent->GetVar<float>(u"rotZ") * pi;
	rotator->mRate = m_Parent->GetVar<float>(u"rate");

	if (m_Path == nullptr || m_Path->pathWaypoints.empty()) return;
	// LWOPlatform::SetupPath 0x00c17d10
	rotator->mTimeBased = m_Path->movingPlatform.timeBasedMovement != 0;
	rotator->mInReverse = m_PathReverse;
	if (static_cast<size_t>(m_StartWaypoint) >= m_Path->pathWaypoints.size()) m_StartWaypoint = 0;
	MoverProcessStateChange(m_StartWaypoint);
	const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
	if (startOnLoad) rotator->mState = ePlatformStateFlag::Travelling;
}

// LWOMovingPlatformComponent::SetCurrentPath 0x00d0f570 (the SetCurrentPath game message, 0x00d0f5d0): the path, the
// waypoint it starts at and its direction, then SetupPlatform 0x00ccea50 makes the subcomponents anew and a mover takes
// the path. Live wrote the path to the clients with the mover (the GF shooting gallery's targets), whose
// LWOMovingPlatformComponent::Deserialize 0x00c88000 does the same there.
bool MovingPlatformComponent::SetCurrentPath(const std::string& pathName, const uint32_t startPoint, const bool reverse) {
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);
	m_PathReverse = reverse;
	m_DirtyPath = true;
	m_PathingStopped = false;

	for (auto& subComponent : m_SubComponents) {
		if (subComponent->GetType() == eMoverSubComponentType::mover) subComponent = std::make_unique<MoverSubComponent>(m_Parent->GetPosition());
		else if (subComponent->GetType() == eMoverSubComponentType::rotater) subComponent = std::make_unique<RotatorSubComponent>(m_Parent->GetPosition());
	}

	const bool mover = IsMover();
	if (mover) {
		m_StartWaypoint = static_cast<size_t>(startPoint) < m_Path->pathWaypoints.size() ? startPoint : 0;
		SetupMoverPath();
		MoveObject();
	} else {
		m_StartWaypoint = startPoint;
		if (m_Path == nullptr) LOG("Cannot setup platform on path %s", pathName.c_str());
		else if (m_MoverSubComponentType == eMoverSubComponentType::rotater) SetupRotator();
	}
	Game::entityManager->SerializeEntity(m_Parent);
	return mover;
}

MovingPlatformComponent::~MovingPlatformComponent() = default;

void MovingPlatformComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	// Live always constructed a simple mover with its starting point (the client builds its path from it), and
	// serialized it only when its state changed: no path, then the one subcomponent and the 0 bit ending the list
	if (m_SubComponents.size() == 1 && m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		auto* const simpleMover = GetSimpleMoverSubComponent();
		const bool dirty = bIsInitialUpdate || simpleMover->mDirtyStartingPoint || simpleMover->mDirtyState;
		outBitStream.Write(dirty);
		outBitStream.Write(bIsInitialUpdate);
		if (bIsInitialUpdate) outBitStream.Write0();
		if (dirty) {
			outBitStream.Write1();
			outBitStream.Write(m_MoverSubComponentType);
			simpleMover->Serialize(outBitStream, bIsInitialUpdate);
			outBitStream.Write0();
		}
		return;
	}

	// LWOMovingPlatformComponent::Deserialize: whether subcomponents follow, whether the path changed (then whether
	// there is one: its name, starting waypoint and reverse), then the subcomponents. Live wrote the path with the
	// construction and the mover's state with every serialization; a path set later (SetCurrentPath) is written once.
	outBitStream.Write<bool>(true);

	const bool writePath = bIsInitialUpdate || m_DirtyPath;
	outBitStream.Write(writePath);
	if (writePath) {
		if (!bIsInitialUpdate) m_DirtyPath = false;
		const auto hasPath = !m_PathName.empty() && m_Path != nullptr;
		outBitStream.Write(hasPath);
		if (hasPath) {
			outBitStream.Write<uint16_t>(m_PathName.size());
			for (const auto& c : m_PathName) {
				outBitStream.Write<uint16_t>(c);
			}
			outBitStream.Write<uint32_t>(m_StartWaypoint);
			outBitStream.Write<bool>(m_PathReverse);
		}
	}

	// The subcomponents, each after a 1 bit with its type (a rotater sends a mover's data, both LWOPlatform's); a 0 bit
	// ends the list. A platform the client made none for gets none: it could not read one.
	for (const auto& subComponent : m_SubComponents) {
		outBitStream.Write1();
		outBitStream.Write(subComponent->GetType());
		subComponent->Serialize(outBitStream, bIsInitialUpdate);
	}
	outBitStream.Write0();
}

void MovingPlatformComponent::OnQuickBuildInitilized() {
	// Live left a quick built simple mover (the NJ counterweights) where it was, only resending its state: scripts
	// send it on
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		GetSimpleMoverSubComponent()->mDirtyState = true;
		return;
	}
	StopPathing();
}

void MovingPlatformComponent::OnCompleteQuickBuild() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// Live (the NJ counterweights, LOT 16141): the built simple mover is serialized waiting at its waypoint (9) and
		// about 0.1 s later travelling (2) to its other end, where it stops without a serialization and
		// PlatformAtLastWaypoint tells the scripts. It is the client's LWOPlatform at work: active once its quick build
		// is complete (SendMessage 0x00bdc930), it waits the shortest wait (SetWaitingTime 0x00bdc860) and sets off
		// (RunPlatform 0x00c86e10): RunSimpleMover.
		GetSimpleMoverSubComponent()->mDirtyState = true;
		return;
	}

	if (m_NoAutoStart)
		return;

	StartPathing();
}

void MovingPlatformComponent::SetMovementState(eMovementPlatformState value) {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		auto* const simpleMover = GetSimpleMoverSubComponent();
		simpleMover->mState = static_cast<uint32_t>(value);
		simpleMover->mDirtyState = true;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (!IsMover()) return;
	GetMoverSubComponent()->mState = static_cast<uint32_t>(value);
	Game::entityManager->SerializeEntity(m_Parent);
}

// Sends the mover to a waypoint, stopping there when stopAtWaypoint: it turns round if the waypoint is behind it. Live
// (the NJ falling tiles): a mover at a waypoint first waits there (9, or 25 at an end, with the desired waypoint), then
// leaves (2).
void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		SimpleMoverGotoWaypoint(static_cast<int32_t>(index));
		return;
	}
	if (!IsMover() || index >= m_Path->pathWaypoints.size()) return;
	auto* const mover = GetMoverSubComponent();
	const bool atWaypoint = (mover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) != 0;
	if (atWaypoint && mover->mCurrentWaypointIndex == index) {
		// Already there
		mover->mDesiredWaypointIndex = -1;
		mover->mShouldStopAtDesiredWaypoint = false;
		if (stopAtWaypoint) {
			mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Travelling)) | ePlatformStateFlag::Stopped;
			mover->mVelocity = NiPoint3Constant::ZERO;
		}
		FireArrivalHooks(index, true, index == m_Path->pathWaypoints.size() - 1);
		return;
	}
	mover->mDesiredWaypointIndex = static_cast<int32_t>(index);
	mover->mShouldStopAtDesiredWaypoint = stopAtWaypoint;

	// LWOPlatform::SetMovingForwards / SetMovingBackwards: at a waypoint it sets off in the waypoint's direction; between
	// two it carries on if the waypoint is ahead, else turns round (the waypoint it was heading to becomes the one it
	// comes from)
	if (atWaypoint) {
		const bool reverse = index < mover->mCurrentWaypointIndex;
		if (reverse != mover->mInReverse) {
			mover->mInReverse = reverse;
			MoverProcessStateChange(mover->mCurrentWaypointIndex);
		}
	} else {
		const bool ahead = mover->mInReverse ? index <= mover->mNextWaypointIndex : index >= mover->mNextWaypointIndex;
		if (!ahead) {
			const float percent = mover->mPercentBetweenPoints;
			const auto heading = mover->mNextWaypointIndex;
			mover->mInReverse = !mover->mInReverse;
			mover->mNextWaypointIndex = mover->mCurrentWaypointIndex;
			mover->mCurrentWaypointIndex = heading;
			mover->mPercentBetweenPoints = 1.0f - std::min(percent, 1.0f);
			mover->mSegmentLength = Vector3::Distance(m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position, m_Path->pathWaypoints[mover->mNextWaypointIndex].position);
			mover->mWaitTime = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].movingPlatform.wait;
			if (mover->mTimeBased) mover->mMoveTimeElapsed = mover->mCurrentSpeed * mover->mPercentBetweenPoints;
			else std::swap(mover->mCurrentSpeed, mover->mNextSpeed);
		}
	}
	MoverResumeTravelling();
	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StartPathing() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// A simple mover's path is start to end: starting sends it to the other end
		SimpleMoverGotoWaypoint(GetSimpleMoverSubComponent()->mCurrentWaypointIndex == 0 ? 1 : 0);
		return;
	}
	if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		// LWOPlatformRotator::StartLinearMovement: spinning again (only on a path, as above)
		if (m_Path == nullptr) return;
		GetMoverSubComponent()->mState = ePlatformStateFlag::Travelling;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (!IsMover()) return;
	m_PathingStopped = false;
	auto* const mover = GetMoverSubComponent();
	if ((mover->mState & ePlatformStateFlag::Travelling) != 0) return;
	// LWOPlatform::ResumeTravelling: stopped between two waypoints it carries on; at a waypoint it first waits there
	// for the waypoint's wait (issue 964: the AG monument elevator waited about 2 s before rising)
	MoverResumeTravelling();
	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StopPathing() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// LWOPlatformSimpleMover::StopPlatform 0x00cce1e0: at an end it is stopped; on its way it goes on to the end it
		// was heading for, where its path (run once) stops it anyway
		auto* const simpleMover = GetSimpleMoverSubComponent();
		if ((simpleMover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) == 0) return;
		simpleMover->mState = (simpleMover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Travelling)) | ePlatformStateFlag::Stopped;
		simpleMover->mDirtyState = true;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		// LWOPlatformRotator::StopPlatform: no longer spinning
		auto* const rotator = GetMoverSubComponent();
		rotator->mState = (rotator->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Travelling)) | ePlatformStateFlag::Stopped;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (!IsMover()) return;
	m_PathingStopped = true;
	auto* const mover = GetMoverSubComponent();
	// LWOPlatform::StopPlatform 0x00c86b70: stopped where it is (the client stops on the stopped bit, 4)
	mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Travelling)) | ePlatformStateFlag::Stopped;
	mover->mVelocity = NiPoint3Constant::ZERO;
	mover->mDesiredWaypointIndex = -1;
	mover->mShouldStopAtDesiredWaypoint = false;
	Game::entityManager->SerializeEntity(m_Parent);
}

bool MovingPlatformComponent::IsMover() const {
	return m_MoverSubComponentType == eMoverSubComponentType::mover && m_Path != nullptr && !m_Path->pathWaypoints.empty();
}

// LWOPlatform::ProcessStateChange: the waypoint becomes the current one, the next follows from the path's behavior and
// the direction (a bounce turns round at either end), with the two waypoints' speeds (GetWaypointSpeed)
void MovingPlatformComponent::MoverProcessStateChange(const uint32_t index) {
	auto* const mover = GetMoverSubComponent();
	const auto& waypoints = m_Path->pathWaypoints;
	const auto count = static_cast<int32_t>(waypoints.size());
	const auto behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
	const auto current = std::min<int32_t>(static_cast<int32_t>(index), count - 1);

	int32_t next{};
	if (!mover->mInReverse) {
		next = current + 1;
		if (next >= count) {
			if (behavior == PathBehavior::Once) next = count - 1;
			else if (behavior == PathBehavior::Bounce) {
				next = count - 2;
				mover->mInReverse = true;
			} else next = 0;
		}
	} else {
		next = current - 1;
		if (next < 0) {
			if (behavior == PathBehavior::Once) next = 0;
			else if (behavior == PathBehavior::Bounce) {
				next = 1;
				mover->mInReverse = false;
			} else next = count - 1;
		}
	}
	next = std::clamp(next, 0, count - 1);

	mover->mCurrentWaypointIndex = current;
	mover->mNextWaypointIndex = next;
	mover->mWaitTime = waypoints[current].movingPlatform.wait;
	mover->mPercentBetweenPoints = 0.0f;
	mover->mMoveTimeElapsed = 0.0f;
	mover->mPosition = waypoints[current].position;
	mover->mSegmentLength = Vector3::Distance(waypoints[current].position, waypoints[next].position);
	if (current == next) {
		mover->mCurrentSpeed = mover->mNextSpeed = 0.0f;
	} else if (mover->mTimeBased) {
		mover->mCurrentSpeed = waypoints[std::min(current, next)].speed;
		mover->mNextSpeed = 0.0f;
	} else {
		mover->mCurrentSpeed = waypoints[current].speed;
		mover->mNextSpeed = waypoints[next].speed;
	}
}

void MovingPlatformComponent::MoverSetWaitingTime(const float waitTime, const float idleTimeElapsed) {
	auto* const mover = GetMoverSubComponent();
	mover->mTimeToWait = std::max(0.1f, waitTime);
	mover->mIdleTimeElapsed = idleTimeElapsed;
	mover->mWaitSet = true;
	// Only at an end of the path, or stopped, does the object lose its velocities here. Waiting at a waypoint in between
	// it keeps them: live's platforms went on as they came for the wait (the GF shooting gallery subs 2 units past each
	// waypoint, the FV dragons 5.33; a dragon met 2.2 s into a 5 s wait was 88 past its waypoint).
	if ((mover->mState & (ePlatformStateFlag::ReachedFinalDestination | ePlatformStateFlag::Stopped)) != 0) mover->mVelocity = NiPoint3Constant::ZERO;
}

void MovingPlatformComponent::MoverUpdateVelocities() {
	auto* const mover = GetMoverSubComponent();
	auto direction = m_Path->pathWaypoints[mover->mNextWaypointIndex].position - m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position;
	const float distance = direction.Length();
	if (distance > 0.0f) direction = direction / distance;
	// LWOPlatform::GetSpeed 0x00bd40e0: a time based path's mCurrentSpeed is the seconds to the next waypoint; else from
	// the waypoint's speed to the next one's along the way
	float speed{};
	if (mover->mTimeBased) speed = mover->mCurrentSpeed > 0.0f ? distance / mover->mCurrentSpeed : 0.0f;
	else speed = mover->mCurrentSpeed + (mover->mNextSpeed - mover->mCurrentSpeed) * mover->mPercentBetweenPoints;
	mover->mVelocity = direction * speed;
	mover->mState &= ePlatformStateFlag::MovementMask;
}

void MovingPlatformComponent::MoverResumeTravelling() {
	auto* const mover = GetMoverSubComponent();
	if ((mover->mState & ePlatformStateFlag::Stopped) != 0 && (mover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) == 0) {
		// LWOPlatform::StartTravelling 0x0058b170
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Travelling;
	}
	if ((mover->mState & ePlatformStateFlag::Travelling) != 0) {
		MoverUpdateVelocities();
	} else {
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Travelling | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Waiting;
		MoverSetWaitingTime(mover->mWaitTime, 0.0f);
	}
}

void MovingPlatformComponent::MoverArrivedAtWaypoint() {
	auto* const mover = GetMoverSubComponent();
	// A platform with nowhere to go (one waypoint, or at the end of a path run once) is not arriving
	if (mover->mCurrentWaypointIndex == mover->mNextWaypointIndex) return;

	const auto& next = m_Path->pathWaypoints[mover->mNextWaypointIndex].position;
	bool closeEnough{};
	if (mover->mTimeBased) {
		// LWOPlatform::CheckEnoughTimeElapsedAtWaypoint 0x00bdc720
		closeEnough = std::abs(mover->mCurrentSpeed - mover->mMoveTimeElapsed) < 0.02f;
	} else {
		// LWOPlatform::CloseToNextWaypoint 0x00bd4140: within 0.01 of it, or past it
		const auto offset = next - mover->mPosition;
		const auto direction = next - m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position;
		closeEnough = offset.SquaredLength() <= 0.0001f || direction.DotProduct(offset) <= 0.0f;
	}
	if (!closeEnough) return;

	const auto arrived = mover->mNextWaypointIndex;
	const bool atLastWaypoint = arrived == static_cast<uint32_t>(m_Path->pathWaypoints.size() - 1);
	bool atDesiredWaypoint = false;
	bool stopAtDesiredWaypoint = false;
	bool turnedAtEnd = false;

	mover->mState = ePlatformStateFlag::ReachedDesiredWaypoint;
	if (mover->mDesiredWaypointIndex == static_cast<int32_t>(arrived)) {
		atDesiredWaypoint = true;
		stopAtDesiredWaypoint = mover->mShouldStopAtDesiredWaypoint;
		mover->mShouldStopAtDesiredWaypoint = false;
		mover->mDesiredWaypointIndex = -1;
	}
	if (arrived == 0 || atLastWaypoint) {
		mover->mState |= ePlatformStateFlag::ReachedFinalDestination;
		if (static_cast<PathBehavior>(m_Path->pathBehavior) == PathBehavior::Once) {
			turnedAtEnd = true;
			mover->mInReverse = !mover->mInReverse;
		}
	}
	if (stopAtDesiredWaypoint || turnedAtEnd) {
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Travelling)) | ePlatformStateFlag::Stopped;
		mover->mVelocity = NiPoint3Constant::ZERO;
	} else {
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Travelling;
	}
	MoverProcessStateChange(arrived);
	MoverHandleWaypointArrived(arrived, atDesiredWaypoint, atLastWaypoint);
}

void MovingPlatformComponent::MoverHandleWaypointArrived(const uint32_t index, const bool atDesiredWaypoint, const bool atLastWaypoint) {
	FireArrivalHooks(index, atDesiredWaypoint, atLastWaypoint);

	// The scripts may have taken the platform or its object away
	auto* const mover = GetMoverSubComponent();
	if (!mover || !IsMover()) return;
	if ((mover->mState & ePlatformStateFlag::Travelling) != 0) {
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Travelling | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Waiting;
		MoverSetWaitingTime(mover->mWaitTime, 0.0f);
	}
	// Live serialized a platform that waits at the waypoint it arrived at (it leaves again), not one that stopped there
	if ((mover->mState & ePlatformStateFlag::Stopped) == 0) Game::entityManager->SerializeEntity(m_Parent);
}

// LWOPlatform moves its object with SetPosition and SetRotation ignoring the dirty flags (ProcessStateChange 0x00cce380,
// Resync 0x00be9310), and its physics carry it between waypoints: live did not serialize a mover's physics for it, each
// client moves its own. The server's object goes with the mover, turned between the two waypoints' rotations.
void MovingPlatformComponent::MoveObject() {
	const auto* const mover = GetMoverSubComponent();
	const auto& from = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].rotation;
	const auto& to = m_Path->pathWaypoints[mover->mNextWaypointIndex].rotation;
	const auto rotation = from == to ? from : glm::slerp(from, to, std::min(mover->mPercentBetweenPoints, 1.0f));
	if (auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>()) {
		simplePhysics->SetPositionAndRotationIgnoringDirtyFlags(mover->mPosition, rotation);
	}
	if (auto* const phantomPhysics = m_Parent->GetComponent<PhantomPhysicsComponent>()) {
		phantomPhysics->SetPositionAndRotationIgnoringDirtyFlags(mover->mPosition, rotation);
	}
}

// LWOPlatform::SetupPath 0x00c17d10 and SendMessage 0x00bdc930 (RebuildNotifyState): a quick build's platform does not
// run until it is built, nor once the quick build is open again
bool MovingPlatformComponent::UpdateActive(PlatformSubComponent& platform) {
	if (!m_CheckedQuickBuild) {
		m_CheckedQuickBuild = true;
		m_QuickBuild = m_Parent->GetComponent<QuickBuildComponent>();
		if (m_QuickBuild) platform.mActive = false;
	}
	if (m_QuickBuild) {
		const auto quickBuildState = m_QuickBuild->GetState();
		if (quickBuildState == eQuickBuildState::OPEN) platform.mActive = false;
		else if (quickBuildState == eQuickBuildState::COMPLETED) platform.mActive = true;
	}
	return platform.mActive;
}

NiPoint3 CalcRotatorAngularVelocity(const float rotX, const float rotY, const float rotZ, const float rate, const bool reverse) {
	// NiMatrix3::MakeXRotation 0x0116c070, MakeYRotation 0x0116c020 and MakeZRotation 0x0116bfd0
	const float sx = static_cast<float>(std::sin(static_cast<double>(rotX))), cx = static_cast<float>(std::cos(static_cast<double>(rotX)));
	const float sy = static_cast<float>(std::sin(static_cast<double>(rotY))), cy = static_cast<float>(std::cos(static_cast<double>(rotY)));
	const float sz = static_cast<float>(std::sin(static_cast<double>(rotZ))), cz = static_cast<float>(std::cos(static_cast<double>(rotZ)));
	const float x[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, cx, sx }, { 0.0f, -sx, cx } };
	const float y[3][3] = { { cy, 0.0f, -sy }, { 0.0f, 1.0f, 0.0f }, { sy, 0.0f, cy } };
	const float z[3][3] = { { cz, sz, 0.0f }, { -sz, cz, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	// NiMatrix3::FromEulerAnglesXYZ 0x0116c3a0: x * (y * z)
	float yz[3][3]{};
	float m[3][3]{};
	for (int32_t i = 0; i < 3; i++) for (int32_t j = 0; j < 3; j++) for (int32_t k = 0; k < 3; k++) yz[i][j] += y[i][k] * z[k][j];
	for (int32_t i = 0; i < 3; i++) for (int32_t j = 0; j < 3; j++) for (int32_t k = 0; k < 3; k++) m[i][j] += x[i][k] * yz[k][j];

	// NiQuaternion::FromRotation
	float q[4]{}; // x, y, z, w
	const float trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0.0f) {
		float root = std::sqrt(trace + 1.0f);
		q[3] = 0.5f * root;
		root = 0.5f / root;
		q[0] = (m[2][1] - m[1][2]) * root;
		q[1] = (m[0][2] - m[2][0]) * root;
		q[2] = (m[1][0] - m[0][1]) * root;
	} else {
		constexpr int32_t next[3] = { 1, 2, 0 };
		int32_t i = 0;
		if (m[1][1] > m[0][0]) i = 1;
		if (m[2][2] > m[i][i]) i = 2;
		const int32_t j = next[i];
		const int32_t k = next[j];
		float root = std::sqrt(m[i][i] - m[j][j] - m[k][k] + 1.0f);
		q[i] = 0.5f * root;
		root = 0.5f / root;
		q[3] = (m[k][j] - m[j][k]) * root;
		q[j] = (m[j][i] + m[i][j]) * root;
		q[k] = (m[k][i] + m[i][k]) * root;
	}

	// hkpKeyFrameUtility::KeyFrameInfo::setUsingPositionOrientationPair 0x012a4050: the rotation from the first to the
	// second (the rotation itself, or its inverse in reverse), normalized; without an axis there is no velocity; else
	// the axis (the other way for a negative w) times twice the arc cosine of |w| over the time
	if (reverse) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
	}
	const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	if (length <= 0.0f || rate == 0.0f) return NiPoint3Constant::ZERO;
	for (auto& part : q) part /= length;
	const float axisLength = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
	if (!(axisLength > std::numeric_limits<float>::epsilon())) return NiPoint3Constant::ZERO;
	const float angle = 2.0f * std::acos(std::min(std::abs(q[3]), 1.0f));
	const float scale = (q[3] < 0.0f ? -1.0f : 1.0f) / axisLength * angle * (1.0f / (rate * 0.5f));
	return NiPoint3(q[0] * scale, q[1] * scale, q[2] * scale);
}

// LWOPlatform::RunPlatform 0x00c86e10 for a simple mover waiting at its waypoint: once the wait is over it sets off for
// its other end (LWOPlatform::IncrementWaitingTime 0x00bd4370, StartTravelling 0x0058b170). A quick build's does not
// run until it is built (LWOPlatform::SendMessage 0x00bdc930). Live: a built counterweight was serialized waiting (9)
// and about 0.1 s later travelling (2).
void MovingPlatformComponent::RunSimpleMover(const float deltaTime) {
	auto* const simpleMover = GetSimpleMoverSubComponent();
	if (!UpdateActive(*simpleMover)) return;
	if ((simpleMover->mState & ePlatformStateFlag::Waiting) == 0 || !simpleMover->mWaitSet) return;
	simpleMover->mIdleTimeElapsed += deltaTime;
	if (simpleMover->mIdleTimeElapsed < simpleMover->mTimeToWait) return;
	simpleMover->mIdleTimeElapsed = 0.0f;
	SimpleMoverGotoWaypoint(simpleMover->mCurrentWaypointIndex == 0 ? 1 : 0);
}

// LWOPlatform::RunPlatform 0x00c86e10 for a rotator: its wait never ends (IncrementWaitingTime 0x004a0390) and it never
// arrives (HandleStoppedLogic 0x00bdc660 keeps the object's angular velocity at the rotator's); it has no linear
// velocity (SetLinearVelocity 0x00bd1660). The client's physics turn the object at that velocity; the server turns its
// object here, without a serialization of its physics, as a mover's.
void MovingPlatformComponent::RunRotator(const float deltaTime) {
	auto* const rotator = static_cast<RotatorSubComponent*>(GetMoverSubComponent());
	if (!rotator || m_Path == nullptr || m_Path->pathWaypoints.empty()) return;

	auto rotation = m_Parent->GetRotation();
	bool moved = false;
	if (!rotator->mPlaced) {
		// LWOPlatform::ProcessStateChange 0x00cce380 from SetupPath: the object is put at its waypoint
		rotator->mPlaced = true;
		rotation = m_Path->pathWaypoints[rotator->mCurrentWaypointIndex].rotation;
		moved = true;
	}
	if (UpdateActive(*rotator) && (rotator->mState & ePlatformStateFlag::Travelling) != 0) {
		const auto velocity = rotator->GetAngularVelocity();
		const float speed = velocity.Length();
		if (speed > 0.0f && std::isfinite(speed)) {
			const glm::vec3 axis(velocity.x / speed, velocity.y / speed, velocity.z / speed);
			rotation = glm::normalize(glm::angleAxis(speed * deltaTime, axis) * rotation);
			moved = true;
		}
	}
	if (!moved) return;
	if (auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>()) {
		simplePhysics->SetPositionAndRotationIgnoringDirtyFlags(rotator->mPosition, rotation);
	}
	if (auto* const phantomPhysics = m_Parent->GetComponent<PhantomPhysicsComponent>()) {
		phantomPhysics->SetPositionAndRotationIgnoringDirtyFlags(rotator->mPosition, rotation);
	}
}

// LWOPlatform::Run 0x00c87080 and RunPlatform 0x00c86e10. The client's platform is moved by its physics, at the
// velocity SetLinearVelocity 0x00be9200 gives it; the server moves it here by the same speed.
void MovingPlatformComponent::Run(const float deltaTime) {
	if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		RunRotator(deltaTime);
		return;
	}
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		RunSimpleMover(deltaTime);
		return;
	}
	if (!IsMover()) return;
	auto* mover = GetMoverSubComponent();
	UpdateActive(*mover);
	if (!mover->mActive) {
		// RebuildNotifyState: an open quick build's platform loses its velocities
		mover->mVelocity = NiPoint3Constant::ZERO;
		return;
	}
	if (mover->mState == 0) return;

	const bool travelling = (mover->mState & ePlatformStateFlag::Travelling) != 0;
	if (mover->mTimeBased && travelling) mover->mMoveTimeElapsed += deltaTime;
	// The object's physics: it goes at its velocity, travelling or not
	mover->mPosition = mover->mPosition + mover->mVelocity * deltaTime;

	if (mover->mTimeBased) {
		// LWOPlatform::TimeBasedCalculatePercentToNextWaypoint 0x00bd4090: mCurrentSpeed is the seconds to the next waypoint
		mover->mMoveTimeElapsed = std::min(mover->mMoveTimeElapsed, mover->mCurrentSpeed);
		mover->mPercentBetweenPoints = mover->mCurrentSpeed > 0.0f ? mover->mMoveTimeElapsed / mover->mCurrentSpeed : 1.0f;
	} else {
		// LWOPlatform::CalculatePercentTravelledToWaypoint 0x00be9050: how far the object is from the current waypoint,
		// over the way to the next (live sent 1.219 for a dragon that far past its waypoint)
		const auto& from = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position;
		mover->mPercentBetweenPoints = mover->mSegmentLength > 0.0f ? Vector3::Distance(from, mover->mPosition) / mover->mSegmentLength : 0.0f;
	}

	MoveObject();

	const bool stopped = (mover->mState & ePlatformStateFlag::Stopped) != 0;
	// LWOPlatform::IncrementWaitingTime 0x00bd4370
	bool doneWaiting = false;
	if ((mover->mState & ePlatformStateFlag::Waiting) != 0 && mover->mWaitSet) {
		mover->mIdleTimeElapsed += deltaTime;
		if (mover->mIdleTimeElapsed >= mover->mTimeToWait) {
			mover->mIdleTimeElapsed = 0.0f;
			doneWaiting = true;
		}
	}
	if (doneWaiting) {
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Travelling;
	} else if (!stopped && (travelling || mover->mSegmentLength > 0.0f)) {
		// LWOPlatform::HandleStoppedLogic 0x00bd16a0: a platform that has not stopped looks whether it arrived, also while
		// it waits. Live's FV dragons, going on as they came through the 5 s wait at their waypoint 4, were at waypoint 5
		// (waiting there, then going on in the direction they came to 4 in) without ever having left 4. Where two
		// waypoints are in one place the client arrives every frame of the wait; here only a travelling one arrives there.
		MoverArrivedAtWaypoint();
		// A script told of the arrival may have put the platform on another path
		mover = GetMoverSubComponent();
		if (!IsMover()) return;
	}

	if ((mover->mState & ePlatformStateFlag::Travelling) != 0 && (doneWaiting || mover->mCurrentSpeed != mover->mNextSpeed)) {
		MoverUpdateVelocities();
		// Live serialized a platform leaving its waypoint, where it then was: as far past the waypoint as the wait took it
		if (doneWaiting) Game::entityManager->SerializeEntity(m_Parent);
	}
	if (IsMover()) MoveObject();
}

void MovingPlatformComponent::FillResync(GameMessages::PlatformResync& resync) const {
	auto* const mover = GetMoverSubComponent();
	resync.bReverse = mover->mInReverse;
	resync.bStopAtDesiredWaypoint = mover->mShouldStopAtDesiredWaypoint;
	resync.eState = static_cast<eMovementPlatformState>(mover->mState);
	resync.fIdleTimeElapsed = mover->mIdleTimeElapsed;
	resync.fMoveTimeElapsed = mover->mMoveTimeElapsed;
	resync.fPercentBetweenPoints = std::min(mover->mPercentBetweenPoints, 1.0f);
	resync.iDesiredWaypointIndex = mover->mDesiredWaypointIndex;
	resync.iIndex = static_cast<int32_t>(mover->mCurrentWaypointIndex);
	resync.iNextIndex = static_cast<int32_t>(mover->mNextWaypointIndex);
	resync.ptUnexpectedLocation = mover->mPosition;
	const auto& from = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].rotation;
	const auto& to = m_Path->pathWaypoints[mover->mNextWaypointIndex].rotation;
	const auto rotation = glm::slerp(glm::quat(from.w, from.x, from.y, from.z), glm::quat(to.w, to.x, to.y, to.z), resync.fPercentBetweenPoints);
	resync.qUnexpectedRotation = NiQuaternion(rotation.w, rotation.x, rotation.y, rotation.z);
}

void MovingPlatformComponent::SetSerialized(bool value) {
	m_Serialize = value;
}

bool MovingPlatformComponent::GetNoAutoStart() const {
	return m_NoAutoStart;
}

void MovingPlatformComponent::SetNoAutoStart(const bool value) {
	m_NoAutoStart = value;
}

void MovingPlatformComponent::WarpToWaypoint(size_t index) {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		auto* const simpleMover = GetSimpleMoverSubComponent();
		m_MoveGeneration++;
		simpleMover->mCurrentWaypointIndex = index == 0 ? 0 : 1;
		simpleMover->mInReverse = simpleMover->mCurrentWaypointIndex == 1;
		simpleMover->mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | ePlatformStateFlag::ReachedFinalDestination;
		simpleMover->mDirtyState = true;
		m_Parent->SetPosition(simpleMover->GetWaypointPosition(simpleMover->mCurrentWaypointIndex));
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (!IsMover() || index >= m_Path->pathWaypoints.size()) return;
	const auto& waypoint = m_Path->pathWaypoints[index];
	MoverProcessStateChange(static_cast<uint32_t>(index));
	auto* const mover = GetMoverSubComponent();
	const bool atEnd = index == 0 || index == m_Path->pathWaypoints.size() - 1;
	mover->mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | (atEnd ? ePlatformStateFlag::ReachedFinalDestination : 0);
	mover->mVelocity = NiPoint3Constant::ZERO;

	m_Parent->SetPosition(waypoint.position);
	m_Parent->SetRotation(waypoint.rotation);

	Game::entityManager->SerializeEntity(m_Parent);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) return 1;
	return m_Path->pathWaypoints.size() - 1;
}

PlatformSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	if (m_SubComponents.empty() || m_MoverSubComponentType == eMoverSubComponentType::simpleMover) return nullptr;
	return m_SubComponents.front().get();
}

SimpleMoverSubComponent* MovingPlatformComponent::GetSimpleMoverSubComponent() const {
	if (m_SubComponents.empty() || m_MoverSubComponentType != eMoverSubComponentType::simpleMover) return nullptr;
	return static_cast<SimpleMoverSubComponent*>(m_SubComponents.front().get());
}

// The client runs the two point path at |platformMove| / platformMoveTime, so it takes platformMoveTime seconds from
// either end. Sending it travelling from its current waypoint towards the other one is all the client needs.
void MovingPlatformComponent::SimpleMoverGotoWaypoint(int32_t index) {
	auto* const simpleMover = GetSimpleMoverSubComponent();
	index = index <= 0 ? 0 : 1;
	const bool travelling = (simpleMover->mState & ePlatformStateFlag::Travelling) != 0;
	if (!travelling && index == simpleMover->mCurrentWaypointIndex) return;

	simpleMover->mCurrentWaypointIndex = index == 1 ? 0 : 1;
	simpleMover->mInReverse = index == 0;
	simpleMover->mState = ePlatformStateFlag::Travelling;
	simpleMover->mDirtyState = true;
	Game::entityManager->SerializeEntity(m_Parent);

	const auto generation = ++m_MoveGeneration;
	m_Parent->AddCallbackTimer(simpleMover->mMoveTime, [this, generation, index] {
		if (generation == m_MoveGeneration) OnSimpleMoverArrived(index);
	});
}

void MovingPlatformComponent::OnSimpleMoverArrived(const int32_t index) {
	auto* const simpleMover = GetSimpleMoverSubComponent();
	if (!simpleMover) return;

	// LWOPlatform::ArrivedAtWaypoint: both ends are the path's ends, and a path run once stops there (28) and turns
	// round. Live did not serialize it (each client's simple mover arrives by itself), and the object is moved ignoring
	// the dirty flags (ProcessStateChange 0x00cce380).
	simpleMover->mCurrentWaypointIndex = index;
	simpleMover->mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | ePlatformStateFlag::ReachedFinalDestination;
	simpleMover->mInReverse = index == 1;
	const auto position = simpleMover->GetWaypointPosition(index);
	if (auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>()) {
		simplePhysics->SetPositionAndRotationIgnoringDirtyFlags(position, simpleMover->mStartRotation);
	}
	if (auto* const phantomPhysics = m_Parent->GetComponent<PhantomPhysicsComponent>()) {
		phantomPhysics->SetPositionAndRotationIgnoringDirtyFlags(position, simpleMover->mStartRotation);
	}

	FireArrivalHooks(index, true, index == 1);
}

// LWOPlatform::HandleWaypointArrived: Arrived, then ArrivedAtDesiredWaypoint at the waypoint it was sent to, then
// PlatformAtLastWaypoint at either end of the path
void MovingPlatformComponent::FireArrivalHooks(const uint32_t index, const bool atDesiredWaypoint, const bool atLastWaypoint) {
	auto* const script = m_Parent->GetScript();
	script->OnWaypointReached(m_Parent, index);
	if (atDesiredWaypoint) script->OnArrivedAtDesiredWaypoint(m_Parent, index);
	if (atLastWaypoint) {
		script->OnPlatformAtLastWaypoint(m_Parent);
		// A callback may add another
		const auto callbacks = m_AtLastWaypointCallbacks;
		for (const auto& callback : callbacks) callback();
	}
}

void MovingPlatformComponent::AddPlatformAtLastWaypointCallback(const std::function<void()>& callback) {
	m_AtLastWaypointCallbacks.push_back(callback);
}

void MovingPlatformComponent::Allocator(Entity& entity, ObjectLoadState& state) {
	const std::string pathName = entity.GetVarAsString(u"attached_path");
	const Path* const path = Game::zoneManager->GetZone()->GetPath(pathName);

	//Check to see if we have an attached path and add the appropiate component to handle it:
	if (path) {
		// if we have a moving platform path, then we need a moving platform component
		if (path->pathType == PathType::MovingPlatform) {
			// The registry component ID picks the subcomponent (a simple mover when it has one), path or not
			entity.AddComponent<MovingPlatformComponent>(state.GetComponentID(eReplicaComponentType::MOVING_PLATFORM, -1), pathName);
		} else if (path->pathType == PathType::Movement) {
			auto* const movementAIcomponent = entity.GetComponent<MovementAIComponent>();
			if (movementAIcomponent && state.GetComponentID(eReplicaComponentType::BASE_COMBAT_AI) == 0) {
				movementAIcomponent->SetPath(pathName);
			} else {
				MovementAIInfo moveInfo{
					.movementType = "",
					.wanderRadius = 16,
					.wanderSpeed = 2.5f,
					.wanderChance = 0,
					.wanderDelayMin = 2,
					.wanderDelayMax = 5,
				};

				entity.AddComponent<MovementAIComponent>(-1, moveInfo);
			}
		}
	} else {
		// else we still need to setup moving platform if it has a moving platform comp but no path
		const auto movingPlatformComponentID = state.GetComponentID(eReplicaComponentType::MOVING_PLATFORM, -1);
		if (movingPlatformComponentID >= 0) {
			entity.AddComponent<MovingPlatformComponent>(movingPlatformComponentID, pathName);
		}
	}
}
