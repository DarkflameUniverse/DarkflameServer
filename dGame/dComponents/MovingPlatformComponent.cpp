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

MoverSubComponent::MoverSubComponent(const NiPoint3& startPos) {
	mPosition = startPos;
}

MoverSubComponent::~MoverSubComponent() = default;

// LWOPlatform::Deserialize 0x00c870e0: a dirty bit, the state, desired waypoint, whether to stop there, reverse, the
// percent between the waypoints, where it is, the current and next waypoints, the time waited and the time travelled
void MoverSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
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
		outBitStream.Write<int32_t>(mCurrentWaypointIndex);
		outBitStream.Write(mInReverse);
		if (!bIsInitialUpdate) mDirtyState = false;
	}
}

NiPoint3 SimpleMoverSubComponent::GetWaypointPosition(const int32_t index) const {
	if (index <= 0) return mStartPosition;
	const glm::vec3 moved = mStartRotation * glm::vec3(mMove.x, mMove.y, mMove.z);
	return mStartPosition + NiPoint3(moved.x, moved.y, moved.z);
}

eMoverSubComponentType ChooseMoverSubComponentType(const int32_t componentID, const Entity& entity) {
	const bool setRotater = entity.HasVar(u"platformIsRotater");
	const bool setMover = entity.HasVar(u"platformIsMover");
	const bool setSimpleMover = entity.HasVar(u"platformIsSimpleMover");

	if (!setRotater && !setMover && !setSimpleMover) {
		return componentID < 1 ? eMoverSubComponentType::mover : eMoverSubComponentType::simpleMover;
	}

	if (setMover && entity.GetVar<bool>(u"platformIsMover")) return eMoverSubComponentType::mover;
	if (setSimpleMover && entity.GetVar<bool>(u"platformIsSimpleMover")) return eMoverSubComponentType::simpleMover;
	if (setRotater && entity.GetVar<bool>(u"platformIsRotater")) return eMoverSubComponentType::rotater;
	return eMoverSubComponentType::none;
}

//------------- MovingPlatformComponent below --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName) : Component(parent, componentID) {
	m_MoverSubComponentType = ChooseMoverSubComponentType(componentID, *m_Parent);
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		auto* const simpleMover = new SimpleMoverSubComponent();
		m_MoverSubComponent = simpleMover;
		simpleMover->mStartPosition = m_Parent->GetDefaultPosition();
		simpleMover->mStartRotation = m_Parent->GetDefaultRotation();

		// LWOPlatformSimpleMover::LoadTemplateData: the MovingPlatforms row, unless the object sets dbonly false
		const bool dbOnly = m_Parent->GetVar<bool>(u"dbonly");
		if (componentID > 0 && (!m_Parent->HasVar(u"dbonly") || dbOnly)) {
			auto query = CDClientDatabase::CreatePreppedStmt(
				"SELECT platformMoveX, platformMoveY, platformMoveZ, platformMoveTime, platformStartAtEnd FROM MovingPlatforms WHERE id = ?;");
			query.bind(1, componentID);
			auto result = query.execQuery();
			if (!result.eof()) {
				simpleMover->mMove = NiPoint3(result.getFloatField(0), result.getFloatField(1), result.getFloatField(2));
				simpleMover->mMoveTime = result.getFloatField(3);
				simpleMover->mStartAtEnd = result.getIntField(4) != 0;
			} else {
				LOG("LOT %i couldn't find simple mover %i in MovingPlatforms", m_Parent->GetLOT(), componentID);
			}
		}

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
	} else {
		m_MoverSubComponent = new MoverSubComponent(m_Parent->GetDefaultPosition());
	}
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);
	m_NoAutoStart = false;

	if (m_Path == nullptr) {
		LOG("Path not found: %s", pathName.c_str());
	}

	// A mover starts at its attached_path_start waypoint, waiting there, and leaves after the waypoint's wait unless
	// startPathingOnLoad is false (live: 25 with the time waited in the constructions)
	if (IsMover()) {
		auto* const mover = GetMoverSubComponent();
		mover->mTimeBased = m_Path->movingPlatform.timeBasedMovement != 0;
		const auto start = m_Parent->HasVar(u"attached_path_start") ? m_Parent->GetVar<int32_t>(u"attached_path_start") : 0;
		m_StartWaypoint = start >= 0 && static_cast<size_t>(start) < m_Path->pathWaypoints.size() ? start : 0;
		MoverProcessStateChange(m_StartWaypoint);
		const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
		MoverStopAtWaypoint(startOnLoad);
	} else if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		// LWOPlatformRotator: it only spins (rotX/rotY/rotZ at rate, its own path never arrives anywhere: its
		// HandleStoppedLogic 0x00bdc660 only keeps the angular velocity), while travelling
		const bool startOnLoad = !m_Parent->HasVar(u"startPathingOnLoad") || m_Parent->GetVar<bool>(u"startPathingOnLoad");
		GetMoverSubComponent()->mState = startOnLoad ? ePlatformStateFlag::Travelling : ePlatformStateFlag::Stopped;
	}

	// Live constructed every moving platform with simple physics as keyframed
	auto* const simplePhysics = m_Parent->GetComponent<SimplePhysicsComponent>();
	if (simplePhysics && !m_Parent->HasVar(u"motionType")) simplePhysics->SetPhysicsMotionState(SimplePhysicsComponent::MOTION_TYPE_KEYFRAMED);
}

MovingPlatformComponent::~MovingPlatformComponent() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) delete static_cast<SimpleMoverSubComponent*>(m_MoverSubComponent);
	else delete static_cast<MoverSubComponent*>(m_MoverSubComponent);
}

void MovingPlatformComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	// Live always constructed a simple mover with its starting point (the client builds its path from it), and
	// serialized it only when its state changed: no path, then the one subcomponent and the 0 bit ending the list
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
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

	// A platform the client made no subcomponent for must not get one: it could not read it
	const auto hasPlatform = m_MoverSubComponentType != eMoverSubComponentType::none;
	outBitStream.Write<bool>(hasPlatform);

	if (hasPlatform) {
		// A rotater's data is a mover's (both read by LWOPlatform::Deserialize)
		auto* mover = static_cast<MoverSubComponent*>(m_MoverSubComponent);
		outBitStream.Write(m_MoverSubComponentType);
		mover->Serialize(outBitStream, bIsInitialUpdate);

		// The client reads subcomponents while a 1 bit comes before one: a 0 bit ends the list
		outBitStream.Write0();
	}
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

// Sends the mover to a waypoint, stopping there when stopAtWaypoint: it turns round if the waypoint is behind it
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
		if (stopAtWaypoint) MoverStopAtWaypoint(false);
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
			if (mover->mTimeBased) mover->mMoveTimeElapsed = mover->mCurrentSpeed * mover->mPercentBetweenPoints;
			else std::swap(mover->mCurrentSpeed, mover->mNextSpeed);
		}
	}
	MoverDepart();
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
	if ((mover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) != 0) {
		MoverStopAtWaypoint(true);
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	MoverDepart();
}

void MovingPlatformComponent::StopPathing() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// LWOPlatformSimpleMover::StopPlatform: it stops where it is, no longer travelling
		auto* const simpleMover = GetSimpleMoverSubComponent();
		m_MoveGeneration++;
		simpleMover->mState = ePlatformStateFlag::Stopped;
		simpleMover->mDirtyState = true;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (m_MoverSubComponentType == eMoverSubComponentType::rotater) {
		// LWOPlatformRotator::StopPlatform: no longer spinning
		GetMoverSubComponent()->mState = ePlatformStateFlag::Stopped;
		Game::entityManager->SerializeEntity(m_Parent);
		return;
	}
	if (!IsMover()) return;
	m_PathingStopped = true;
	auto* const mover = GetMoverSubComponent();
	// LWOPlatform::StopPlatform: stopped where it is, not leaving after a wait
	mover->mState = (mover->mState & ~(ePlatformStateFlag::Travelling | ePlatformStateFlag::Waiting)) | ePlatformStateFlag::Stopped;
	mover->mDepartAfterWait = false;
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

void MovingPlatformComponent::MoverStopAtWaypoint(const bool depart) {
	auto* const mover = GetMoverSubComponent();
	const auto last = static_cast<uint32_t>(m_Path->pathWaypoints.size() - 1);
	const bool atEnd = mover->mCurrentWaypointIndex == 0 || mover->mCurrentWaypointIndex == last;
	mover->mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint | (atEnd ? ePlatformStateFlag::ReachedFinalDestination : 0);
	mover->mIdleTimeElapsed = 0.0f;
	// LWOPlatform::SetWaitingTime: at least 0.1 s
	mover->mTimeToWait = std::max(0.1f, m_Path->pathWaypoints[mover->mCurrentWaypointIndex].movingPlatform.wait);
	mover->mDepartAfterWait = depart;
}

void MovingPlatformComponent::MoverDepart() {
	auto* const mover = GetMoverSubComponent();
	mover->mState = ePlatformStateFlag::Travelling;
	mover->mIdleTimeElapsed = 0.0f;
	mover->mDepartAfterWait = false;
	Game::entityManager->SerializeEntity(m_Parent);
}

// LWOPlatform::ArrivedAtWaypoint and HandleWaypointArrived: at the next waypoint, it stops there if it was sent there
// to stop, or at an end of a path run once (which turns it round); otherwise it waits there and goes on
void MovingPlatformComponent::MoverArrive() {
	auto* const mover = GetMoverSubComponent();
	const auto arrived = mover->mNextWaypointIndex;
	const auto last = static_cast<uint32_t>(m_Path->pathWaypoints.size() - 1);
	const bool atEnd = arrived == last;
	const bool atDesired = mover->mDesiredWaypointIndex == static_cast<int32_t>(arrived);
	bool stop = false;
	if (atDesired) {
		stop = mover->mShouldStopAtDesiredWaypoint;
		mover->mShouldStopAtDesiredWaypoint = false;
		mover->mDesiredWaypointIndex = -1;
	}
	if ((arrived == 0 || atEnd) && static_cast<PathBehavior>(m_Path->pathBehavior) == PathBehavior::Once) {
		stop = true;
		mover->mInReverse = !mover->mInReverse;
	}
	MoverProcessStateChange(arrived);
	MoverStopAtWaypoint(!stop);
	Game::entityManager->SerializeEntity(m_Parent);
	FireArrivalHooks(arrived, atDesired, atEnd);
}

void MovingPlatformComponent::Run(const float deltaTime) {
	if (!IsMover()) return;
	auto* const mover = GetMoverSubComponent();
	if ((mover->mState & ePlatformStateFlag::Travelling) != 0) {
		// LWOPlatform::Run / RunPlatform: time based paths count the time travelled; others move at the speed between
		// the two waypoints' (CalculateSpeed), the client by its physics
		if (mover->mTimeBased) {
			mover->mMoveTimeElapsed += deltaTime;
			mover->mPercentBetweenPoints = mover->mCurrentSpeed > 0.0f ? std::min(mover->mMoveTimeElapsed, mover->mCurrentSpeed) / mover->mCurrentSpeed : 1.0f;
		} else if (mover->mSegmentLength > 0.0f) {
			const float speed = mover->mCurrentSpeed + (mover->mNextSpeed - mover->mCurrentSpeed) * mover->mPercentBetweenPoints;
			mover->mPercentBetweenPoints += std::max(speed, 0.0f) * deltaTime / mover->mSegmentLength;
		} else {
			mover->mPercentBetweenPoints = 1.0f;
		}
		const auto& from = m_Path->pathWaypoints[mover->mCurrentWaypointIndex].position;
		const auto& to = m_Path->pathWaypoints[mover->mNextWaypointIndex].position;
		const float percent = std::min(mover->mPercentBetweenPoints, 1.0f);
		mover->mPosition = from + (to - from) * percent;
		if (mover->mPercentBetweenPoints >= 1.0f) MoverArrive();
	} else if (mover->mDepartAfterWait) {
		mover->mIdleTimeElapsed += deltaTime;
		if (mover->mIdleTimeElapsed >= mover->mTimeToWait) MoverDepart();
	} else if ((mover->mState & ePlatformStateFlag::ReachedDesiredWaypoint) != 0) {
		mover->mIdleTimeElapsed += deltaTime;
	}
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
	MoverStopAtWaypoint(false);

	m_Parent->SetPosition(waypoint.position);
	m_Parent->SetRotation(waypoint.rotation);

	Game::entityManager->SerializeEntity(m_Parent);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) return 1;
	return m_Path->pathWaypoints.size() - 1;
}

MoverSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) return nullptr;
	return static_cast<MoverSubComponent*>(m_MoverSubComponent);
}

SimpleMoverSubComponent* MovingPlatformComponent::GetSimpleMoverSubComponent() const {
	if (m_MoverSubComponentType != eMoverSubComponentType::simpleMover) return nullptr;
	return static_cast<SimpleMoverSubComponent*>(m_MoverSubComponent);
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
