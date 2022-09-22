/*
 * Darkflame Universe
 * Copyright 2019
 */

#include "MovingPlatformComponent.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "SimplePhysicsComponent.h"

MoverSubComponent::MoverSubComponent(const NiPoint3& startPos) {
	mPosition = {};

	mState = MovementPlatformState::Stopped;
	mDesiredWaypointIndex = 0; // -1;
	mInReverse = false;
	mShouldStopAtDesiredWaypoint = false;

	mPercentBetweenPoints = 0.0f;

	mCurrentWaypointIndex = 0;
	mNextWaypointIndex = 0; //mCurrentWaypointIndex + 1;

	mIdleTimeElapsed = 0.0f;
}

MoverSubComponent::~MoverSubComponent() = default;

void MoverSubComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) const {
	outBitStream->Write<bool>(true);

	outBitStream->Write<uint32_t>(static_cast<uint32_t>(mState));
	outBitStream->Write<int32_t>(mDesiredWaypointIndex);
	outBitStream->Write(mShouldStopAtDesiredWaypoint);
	outBitStream->Write(mInReverse);

	outBitStream->Write<float_t>(mPercentBetweenPoints);

	outBitStream->Write<float_t>(mPosition.x);
	outBitStream->Write<float_t>(mPosition.y);
	outBitStream->Write<float_t>(mPosition.z);

	outBitStream->Write<uint32_t>(mCurrentWaypointIndex);
	outBitStream->Write<uint32_t>(mNextWaypointIndex);

	outBitStream->Write<float_t>(mIdleTimeElapsed);
	outBitStream->Write<float_t>(0.0f); // Move time elapsed
}

//------------- MovingPlatformComponent below --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const std::string& pathName) : Component(parent) {
	m_MoverSubComponentType = eMoverSubComponentType::mover;
	m_MoverSubComponent = new MoverSubComponent(m_Parent->GetDefaultPosition());
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = dZoneManager::Instance()->GetZone()->GetPath(pathName);
	m_NoAutoStart = false;

	if (m_Path == nullptr) {
		Game::logger->Log("MovingPlatformComponent", "Path not found: %s", pathName.c_str());
	}
}

MovingPlatformComponent::~MovingPlatformComponent() {
	delete static_cast<MoverSubComponent*>(m_MoverSubComponent);
}

void MovingPlatformComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	// Here we don't serialize the moving platform to let the client simulate the movement

	if (!m_Serialize) {
		outBitStream->Write<bool>(false);
		outBitStream->Write<bool>(false);

		return;
	}

	outBitStream->Write<bool>(true);

	auto hasPath = !m_PathingStopped && !m_PathName.empty();
	outBitStream->Write(hasPath);

	if (hasPath) {
		// Is on rail
		outBitStream->Write1();

		outBitStream->Write(static_cast<uint16_t>(m_PathName.size()));
		for (const auto& c : m_PathName) {
			outBitStream->Write(static_cast<uint16_t>(c));
		}

		// Starting point
		outBitStream->Write<uint32_t>(0);

		// Reverse
		outBitStream->Write<bool>(false);
	}

	const auto hasPlatform = m_MoverSubComponent != nullptr;
	outBitStream->Write<bool>(hasPlatform);

	if (hasPlatform) {
		auto* mover = static_cast<MoverSubComponent*>(m_MoverSubComponent);
		outBitStream->Write<uint32_t>(static_cast<uint32_t>(m_MoverSubComponentType));

		if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
			// TODO
		} else {
			mover->Serialize(outBitStream, bIsInitialUpdate, flags);
		}
	}
}

void MovingPlatformComponent::OnRebuildInitilized() {
	StopPathing();
}

void MovingPlatformComponent::OnCompleteRebuild() {
	if (m_NoAutoStart)
		return;

	StartPathing();
}

void MovingPlatformComponent::SetMovementState(MovementPlatformState value) {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mState = value;

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mDesiredWaypointIndex = index;
	subComponent->mNextWaypointIndex = index;
	subComponent->mShouldStopAtDesiredWaypoint = stopAtWaypoint;

	StartPathing();
}

void MovingPlatformComponent::StartPathing() {
	//GameMessages::SendStartPathing(m_Parent);
	m_PathingStopped = false;

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mShouldStopAtDesiredWaypoint = true;
	subComponent->mState = MovementPlatformState::Stationary;

	NiPoint3 targetPosition;

	if (m_Path != nullptr) {
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
		const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

		subComponent->mPosition = currentWaypoint.position;
		subComponent->mSpeed = currentWaypoint.movingPlatform.speed;
		subComponent->mWaitTime = currentWaypoint.movingPlatform.wait;

		targetPosition = nextWaypoint.position;
	} else {
		subComponent->mPosition = m_Parent->GetPosition();
		subComponent->mSpeed = 1.0f;
		subComponent->mWaitTime = 2.0f;

		targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);
	}

	m_Parent->AddCallbackTimer(subComponent->mWaitTime, [this] {
		SetMovementState(MovementPlatformState::Moving);
		});

	const auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5f;

	const auto travelNext = subComponent->mWaitTime + travelTime;

	m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(m_Parent)) {
			script->OnWaypointReached(m_Parent, subComponent->mNextWaypointIndex);
		}
		});

	m_Parent->AddCallbackTimer(travelNext, [this] {
		ContinuePathing();
		});

	//GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::ContinuePathing() {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	subComponent->mState = MovementPlatformState::Stationary;

	subComponent->mCurrentWaypointIndex = subComponent->mNextWaypointIndex;

	NiPoint3 targetPosition;
	uint32_t pathSize;
	PathBehavior behavior;

	if (m_Path != nullptr) {
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
		const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

		subComponent->mPosition = currentWaypoint.position;
		subComponent->mSpeed = currentWaypoint.movingPlatform.speed;
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
			EntityManager::Instance()->SerializeEntity(m_Parent);
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
		SetMovementState(MovementPlatformState::Moving);
		});

	auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5;

	if (m_Parent->GetLOT() == 9483) {
		travelTime += 20;
	}

	const auto travelNext = subComponent->mWaitTime + travelTime;

	m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(m_Parent)) {
			script->OnWaypointReached(m_Parent, subComponent->mNextWaypointIndex);
		}
		});

	m_Parent->AddCallbackTimer(travelNext, [this] {
		ContinuePathing();
		});

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StopPathing() {
	//m_Parent->CancelCallbackTimers();

	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	m_PathingStopped = true;

	subComponent->mState = MovementPlatformState::Stopped;
	subComponent->mDesiredWaypointIndex = -1;
	subComponent->mShouldStopAtDesiredWaypoint = false;

	EntityManager::Instance()->SerializeEntity(m_Parent);

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
	const auto& waypoint = m_Path->pathWaypoints[index];

	m_Parent->SetPosition(waypoint.position);
	m_Parent->SetRotation(waypoint.rotation);

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	return m_Path->pathWaypoints.size() - 1;
}

MoverSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	return static_cast<MoverSubComponent*>(m_MoverSubComponent);
}
