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

#include <glm/gtc/quaternion.hpp>

MoverSubComponent::MoverSubComponent(const NiPoint3& startPos) {
	mPosition = {};

	mState = eMovementPlatformState::Stopped;
	mDesiredWaypointIndex = 0; // -1;
	mInReverse = false;
	mShouldStopAtDesiredWaypoint = false;

	mPercentBetweenPoints = 0.0f;

	mCurrentWaypointIndex = 0;
	mNextWaypointIndex = 0; //mCurrentWaypointIndex + 1;

	mIdleTimeElapsed = 0.0f;
}

MoverSubComponent::~MoverSubComponent() = default;

void MoverSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write<bool>(true);

	outBitStream.Write(mState);
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
	outBitStream.Write<float_t>(0.0f); // Move time elapsed
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

	// Here we don't serialize the moving platform to let the client simulate the movement

	if (!m_Serialize) {
		outBitStream.Write<bool>(false);
		outBitStream.Write<bool>(false);

		return;
	}

	outBitStream.Write<bool>(true);

	auto hasPath = !m_PathingStopped && !m_PathName.empty();
	outBitStream.Write(hasPath);

	if (hasPath) {
		// Is on rail
		outBitStream.Write1();

		outBitStream.Write<uint16_t>(m_PathName.size());
		for (const auto& c : m_PathName) {
			outBitStream.Write<uint16_t>(c);
		}

		// Starting point
		outBitStream.Write<uint32_t>(0);

		// Reverse
		outBitStream.Write<bool>(false);
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

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mState = value;

	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		SimpleMoverGotoWaypoint(static_cast<int32_t>(index));
		return;
	}

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mDesiredWaypointIndex = index;
	subComponent->mNextWaypointIndex = index;
	subComponent->mShouldStopAtDesiredWaypoint = stopAtWaypoint;

	StartPathing();
}

void MovingPlatformComponent::StartPathing() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// A simple mover's path is start to end: starting sends it to the other end
		SimpleMoverGotoWaypoint(GetSimpleMoverSubComponent()->mCurrentWaypointIndex == 0 ? 1 : 0);
		return;
	}
	//GameMessages::SendStartPathing(m_Parent);
	m_PathingStopped = false;

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mShouldStopAtDesiredWaypoint = true;
	subComponent->mState = eMovementPlatformState::Stationary;

	NiPoint3 targetPosition;

	if (m_Path != nullptr) {
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
		const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

		subComponent->mPosition = currentWaypoint.position;
		subComponent->mSpeed = currentWaypoint.speed;
		subComponent->mWaitTime = currentWaypoint.movingPlatform.wait;

		targetPosition = nextWaypoint.position;
	} else {
		subComponent->mPosition = m_Parent->GetPosition();
		subComponent->mSpeed = 1.0f;
		subComponent->mWaitTime = 2.0f;

		targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);
	}

	m_Parent->AddCallbackTimer(subComponent->mWaitTime, [this] {
		SetMovementState(eMovementPlatformState::Moving);
		});

	const auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5f;

	const auto travelNext = subComponent->mWaitTime + travelTime;

	m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
		FireArrivalHooks(subComponent->mNextWaypointIndex, subComponent->mNextWaypointIndex == static_cast<uint32_t>(subComponent->mDesiredWaypointIndex));
		});

	m_Parent->AddCallbackTimer(travelNext, [this] {
		ContinuePathing();
		});

	//GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);

	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::ContinuePathing() {
	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) return;
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mState = eMovementPlatformState::Stationary;

	subComponent->mCurrentWaypointIndex = subComponent->mNextWaypointIndex;

	NiPoint3 targetPosition;
	uint32_t pathSize;
	PathBehavior behavior;

	if (m_Path != nullptr) {
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
		const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

		subComponent->mPosition = currentWaypoint.position;
		subComponent->mSpeed = currentWaypoint.speed;
		subComponent->mWaitTime = currentWaypoint.movingPlatform.wait; // + 2;

		pathSize = m_Path->pathWaypoints.size() - 1;

		behavior = static_cast<PathBehavior>(m_Path->pathBehavior);

		targetPosition = nextWaypoint.position;
	} else {
		subComponent->mPosition = m_Parent->GetPosition();
		subComponent->mSpeed = 1.0f;
		subComponent->mWaitTime = 2.0f;

		targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);

		pathSize = 1;
		behavior = PathBehavior::Loop;
	}

	if (m_Parent->GetLOT() == 9483) {
		behavior = PathBehavior::Bounce;
	} else {
		return;
	}

	if (subComponent->mCurrentWaypointIndex >= pathSize) {
		subComponent->mCurrentWaypointIndex = pathSize;
		switch (behavior) {
		case PathBehavior::Once:
			Game::entityManager->SerializeEntity(m_Parent);
			return;

		case PathBehavior::Bounce:
			subComponent->mInReverse = true;
			break;

		case PathBehavior::Loop:
			subComponent->mNextWaypointIndex = 0;
			break;

		default:
			break;
		}
	} else if (subComponent->mCurrentWaypointIndex == 0) {
		subComponent->mInReverse = false;
	}

	if (subComponent->mInReverse) {
		subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex - 1;
	} else {
		subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex + 1;
	}

	/*
	subComponent->mNextWaypointIndex = 0;
	subComponent->mCurrentWaypointIndex = 1;
	*/

	//GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);

	if (subComponent->mCurrentWaypointIndex == subComponent->mDesiredWaypointIndex) {
		// TODO: Send event?
		StopPathing();

		return;
	}

	m_Parent->CancelCallbackTimers();

	m_Parent->AddCallbackTimer(subComponent->mWaitTime, [this] {
		SetMovementState(eMovementPlatformState::Moving);
		});

	auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5;

	if (m_Parent->GetLOT() == 9483) {
		travelTime += 20;
	}

	const auto travelNext = subComponent->mWaitTime + travelTime;

	m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
		FireArrivalHooks(subComponent->mNextWaypointIndex, subComponent->mNextWaypointIndex == static_cast<uint32_t>(subComponent->mDesiredWaypointIndex));
		});

	m_Parent->AddCallbackTimer(travelNext, [this] {
		ContinuePathing();
		});

	Game::entityManager->SerializeEntity(m_Parent);
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
	//m_Parent->CancelCallbackTimers();

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	m_PathingStopped = true;

	subComponent->mState = eMovementPlatformState::Stopped;
	subComponent->mDesiredWaypointIndex = -1;
	subComponent->mShouldStopAtDesiredWaypoint = false;

	Game::entityManager->SerializeEntity(m_Parent);

	//GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);
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
	const auto& waypoint = m_Path->pathWaypoints[index];

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

	FireArrivalHooks(index, true);
}

// LWOPlatform::HandleWaypointArrived: Arrived, then ArrivedAtDesiredWaypoint at the waypoint it was sent to, then
// PlatformAtLastWaypoint at either end of the path
void MovingPlatformComponent::FireArrivalHooks(const uint32_t index, const bool atDesiredWaypoint) {
	auto* const script = m_Parent->GetScript();
	script->OnWaypointReached(m_Parent, index);
	if (atDesiredWaypoint) script->OnArrivedAtDesiredWaypoint(m_Parent, index);
	const size_t last = m_MoverSubComponentType == eMoverSubComponentType::simpleMover ? 1 : (m_Path ? m_Path->pathWaypoints.size() - 1 : 0);
	if (index == 0 || index == last) script->OnPlatformAtLastWaypoint(m_Parent);
}
