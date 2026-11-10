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
	}

	const auto pathName = GeneralUtils::UTF16ToWTF8(m_PathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);

	if (m_Path == nullptr) {
		LOG("Path not found: %s", pathName.c_str());
	}

	// LWOPlatform::SetupPath: a mover is at its attached_path_start waypoint, stopped (12, as live constructed movers
	// nothing had started). Unless startPathingOnLoad is false it then waits the waypoint's wait and leaves (live: 9).
	if (IsMover()) {
		auto* const mover = GetMoverSubComponent();
		mover->mTimeBased = m_Path->movingPlatform.timeBasedMovement != 0;
		const auto start = m_Parent->HasVar(u"attached_path_start") ? m_Parent->GetVar<int32_t>(u"attached_path_start") : 0;
		m_StartWaypoint = start >= 0 && static_cast<size_t>(start) < m_Path->pathWaypoints.size() ? start : 0;
		MoverProcessStateChange(m_StartWaypoint);
		const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
		if (startOnLoad) MoverResumeTravelling();
	} else if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		// LWOPlatformRotator: it only spins (rotX/rotY/rotZ at rate, its own path never arrives anywhere: its
		// HandleStoppedLogic 0x00bdc660 only keeps the angular velocity), while travelling
		const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
		if (startOnLoad) GetMoverSubComponent()->mState = ePlatformStateFlag::Travelling;
	}

	// Live constructed every moving platform with simple physics as keyframed
	auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>();
	if (simplePhysics && !m_Parent->HasVar(u"motionType")) simplePhysics->SetPhysicsMotionState(SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);
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
	// construction and the mover's state with every serialization.
	outBitStream.Write<bool>(true);

	outBitStream.Write(bIsInitialUpdate);
	if (bIsInitialUpdate) {
		const auto hasPath = !m_PathName.empty() && m_Path != nullptr;
		outBitStream.Write(hasPath);
		if (hasPath) {
			outBitStream.Write<uint16_t>(m_PathName.size());
			for (const auto& c : m_PathName) {
				outBitStream.Write<uint16_t>(c);
			}
			outBitStream.Write<uint32_t>(m_StartWaypoint);
			outBitStream.Write<bool>(false);
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
		// LWOPlatformRotator::StartLinearMovement: spinning again
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
}

void MovingPlatformComponent::MoverResumeTravelling() {
	auto* const mover = GetMoverSubComponent();
	if ((mover->mState & ePlatformStateFlag::Stopped) != 0 && (mover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) == 0) {
		// LWOPlatform::StartTravelling 0x0058b170
		mover->mState = (mover->mState & ~(ePlatformStateFlag::Waiting | ePlatformStateFlag::Stopped)) | ePlatformStateFlag::Travelling;
	}
	if ((mover->mState & ePlatformStateFlag::Travelling) != 0) {
		// LWOPlatform::UpdateVelocities
		mover->mState &= ePlatformStateFlag::MovementMask;
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

// LWOPlatform::Run 0x00c87080 and RunPlatform 0x00c86e10. The client's platform is moved by its physics, at the
// velocity SetLinearVelocity 0x00be9200 gives it; the server moves it here by the same speed.
void MovingPlatformComponent::Run(const float deltaTime) {
	if (!IsMover()) return;
	auto* mover = GetMoverSubComponent();

	// LWOPlatform::SetupPath 0x00c17d10 and SendMessage 0x00bdc930 (RebuildNotifyState): a quick build's platform does not
	// run until it is built, nor once the quick build is open again
	if (!m_CheckedQuickBuild) {
		m_CheckedQuickBuild = true;
		m_QuickBuild = m_Parent->GetComponent<QuickBuildComponent>();
		if (m_QuickBuild) mover->mActive = false;
	}
	if (m_QuickBuild) {
		const auto quickBuildState = m_QuickBuild->GetState();
		if (quickBuildState == eQuickBuildState::OPEN) mover->mActive = false;
		else if (quickBuildState == eQuickBuildState::COMPLETED) mover->mActive = true;
	}
	if (!mover->mActive || mover->mState == 0) return;

	const bool travelling = (mover->mState & ePlatformStateFlag::Travelling) != 0;
	const auto& from = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position;
	const auto& to = m_Path->pathWaypoints[mover->mNextWaypointIndex].position;
	if (mover->mTimeBased) {
		// LWOPlatform::TimeBasedCalculatePercentToNextWaypoint 0x00bd4090: mCurrentSpeed is the seconds to the next waypoint
		if (travelling) mover->mMoveTimeElapsed += deltaTime;
		mover->mMoveTimeElapsed = std::min(mover->mMoveTimeElapsed, mover->mCurrentSpeed);
		mover->mPercentBetweenPoints = mover->mCurrentSpeed > 0.0f ? mover->mMoveTimeElapsed / mover->mCurrentSpeed : 1.0f;
		if (travelling) mover->mPosition = from + (to - from) * mover->mPercentBetweenPoints;
	} else if (travelling) {
		// LWOPlatform::GetSpeed 0x00bd40e0: from the waypoint's speed to the next one's along the way
		if (mover->mSegmentLength > 0.0f) {
			const float speed = mover->mCurrentSpeed + (mover->mNextSpeed - mover->mCurrentSpeed) * mover->mPercentBetweenPoints;
			const float travelled = mover->mPercentBetweenPoints * mover->mSegmentLength + std::max(speed, 0.0f) * deltaTime;
			mover->mPercentBetweenPoints = std::min(travelled / mover->mSegmentLength, 1.0f);
		} else {
			mover->mPercentBetweenPoints = 1.0f;
		}
		mover->mPosition = from + (to - from) * mover->mPercentBetweenPoints;
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
	} else if (!stopped && travelling) {
		// LWOPlatform::HandleStoppedLogic 0x00bd16a0. The client also checks while it waits, which only matters where
		// two waypoints are in one place: there it would arrive every frame.
		MoverArrivedAtWaypoint();
		// A script told of the arrival may have put the platform on another path
		mover = GetMoverSubComponent();
		if (!IsMover()) return;
	}

	if ((mover->mState & ePlatformStateFlag::Travelling) != 0 && (doneWaiting || mover->mCurrentSpeed != mover->mNextSpeed)) {
		mover->mState &= ePlatformStateFlag::MovementMask;
		// Live serialized a platform leaving its waypoint
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

	// LWOPlatform::ArrivedAtWaypoint: both ends are the path's ends, and a path run once stops there
	simpleMover->mCurrentWaypointIndex = index;
	simpleMover->mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | ePlatformStateFlag::ReachedFinalDestination;
	simpleMover->mInReverse = index == 1;
	simpleMover->mDirtyState = true;
	m_Parent->SetPosition(simpleMover->GetWaypointPosition(index));
	Game::entityManager->SerializeEntity(m_Parent);

	FireArrivalHooks(index, true, index == 1);
}

// LWOPlatform::HandleWaypointArrived: Arrived, then ArrivedAtDesiredWaypoint at the waypoint it was sent to, then
// PlatformAtLastWaypoint at either end of the path
void MovingPlatformComponent::FireArrivalHooks(const uint32_t index, const bool atDesiredWaypoint, const bool atLastWaypoint) {
	auto* const script = m_Parent->GetScript();
	script->OnWaypointReached(m_Parent, index);
	if (atDesiredWaypoint) script->OnArrivedAtDesiredWaypoint(m_Parent, index);
	if (atLastWaypoint) script->OnPlatformAtLastWaypoint(m_Parent);
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
