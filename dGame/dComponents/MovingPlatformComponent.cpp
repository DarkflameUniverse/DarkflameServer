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
#include "CDClientManager.h"
#include "CDMovingPlatformComponentTable.h"
#include "Zone.h"

 //------------- PlatformSubComponent begin --------------

PlatformSubComponent::PlatformSubComponent(MovingPlatformComponent* parentComponent) {
	m_Position = NiPoint3::ZERO;
	m_ParentComponent = parentComponent;

	m_State = eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint;
	m_DesiredWaypointIndex = 0;
	m_InReverse = false;
	m_ShouldStopAtDesiredWaypoint = false;

	m_PercentBetweenPoints = 0.0f;

	m_CurrentWaypointIndex = 0;
	m_NextWaypointIndex = 0;

	m_IdleTimeElapsed = 0.0f;
}

void PlatformSubComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(bIsInitialUpdate || m_IsDirty);
	if (!(bIsInitialUpdate || m_IsDirty)) return;
	outBitStream->Write(m_State);
	outBitStream->Write(m_DesiredWaypointIndex);
	outBitStream->Write(m_ShouldStopAtDesiredWaypoint);
	outBitStream->Write(m_InReverse);
	outBitStream->Write(m_PercentBetweenPoints);
	outBitStream->Write(m_Position.x);
	outBitStream->Write(m_Position.y);
	outBitStream->Write(m_Position.z);
	outBitStream->Write(m_CurrentWaypointIndex);
	outBitStream->Write(m_NextWaypointIndex);
	outBitStream->Write(m_IdleTimeElapsed);
	outBitStream->Write(m_MoveTimeElapsed);
	if (!bIsInitialUpdate) m_IsDirty = false;
}

//------------- PlatformSubComponent end --------------

//------------- MoverPlatformSubComponent begin --------------

MoverPlatformSubComponent::MoverPlatformSubComponent(MovingPlatformComponent* parentComponent) : PlatformSubComponent(parentComponent) {

}

//------------- MoverPlatformSubComponent end --------------

//------------- RotatorPlatformSubComponent begin --------------

RotatorPlatformSubComponent::RotatorPlatformSubComponent(MovingPlatformComponent* parentComponent) : PlatformSubComponent(parentComponent) {

}

//------------- RotatorPlatformSubComponent end --------------

//------------- SimpleMoverPlatformSubComponent begin --------------

void SimpleMoverPlatformSubComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyStartingPoint);
	if (bIsInitialUpdate || m_DirtyStartingPoint) {
		outBitStream->Write(m_HasStartingPoint);
		if (m_HasStartingPoint) {
			outBitStream->Write(m_StartingPoint.x);
			outBitStream->Write(m_StartingPoint.y);
			outBitStream->Write(m_StartingPoint.z);
			outBitStream->Write(m_StartingRotation.w);
			outBitStream->Write(m_StartingRotation.x);
			outBitStream->Write(m_StartingRotation.y);
			outBitStream->Write(m_StartingRotation.z);
		}
		if (!bIsInitialUpdate) m_DirtyStartingPoint = false;
	}

	outBitStream->Write(bIsInitialUpdate || m_IsDirty);
	if (bIsInitialUpdate || m_IsDirty) {
		outBitStream->Write(m_State);
		outBitStream->Write(m_CurrentWaypointIndex);
		outBitStream->Write(m_InReverse);
		if (!bIsInitialUpdate) m_IsDirty = false;
	}
}

void SimpleMoverPlatformSubComponent::LoadConfigData() {
	if (m_ParentComponent->GetParent()->GetVar<bool>(u"dbonly")) return;
	NiPoint3 platformMove(
		m_ParentComponent->GetParent()->GetVar<float>(u"platformMoveX"),
		m_ParentComponent->GetParent()->GetVar<float>(u"platformMoveY"),
		m_ParentComponent->GetParent()->GetVar<float>(u"platformMoveZ")
	);
	m_PlatformMove = platformMove;
	m_MoveTime = m_ParentComponent->GetParent()->GetVar<float>(u"platformMoveTime");
	// idk either. client does it!
	m_StartAtEnd = m_ParentComponent->GetParent()->GetVar<uint32_t>(u"attached_path_start") != 0;
	m_StartAtEnd = m_ParentComponent->GetParent()->GetVar<bool>(u"platformStartAtEnd");
}

void SimpleMoverPlatformSubComponent::LoadDataFromTemplate() {
	if (!m_ParentComponent->GetParent()->GetVar<bool>(u"dbonly")) return;

	auto* movingPlatformTable = CDClientManager::Instance().GetTable<CDMovingPlatformComponentTable>();
	if (movingPlatformTable == nullptr) return;

	const auto& platformEntry = movingPlatformTable->GetPlatformEntry(m_ParentComponent->GetComponentId());
	if (!platformEntry || !platformEntry->platformIsSimpleMover) return;

	NiPoint3 platformMove = platformEntry->platformMove;
	float moveTime = platformEntry->moveTime;
}

SimpleMoverPlatformSubComponent::SimpleMoverPlatformSubComponent(MovingPlatformComponent* parentComponent, const NiPoint3& platformMove, const bool startsInReverse) : PlatformSubComponent(parentComponent) {
	m_PlatformMove = platformMove;
	m_InReverse = startsInReverse;
	m_HasStartingPoint = true;
	m_DirtyStartingPoint = true;
	m_IsDirty = true;
	m_StartingPoint = m_ParentComponent->GetParent()->GetPosition();
	m_StartingRotation = m_ParentComponent->GetParent()->GetRotation();
}

//------------- SimpleMoverPlatformSubComponent end --------------

//------------- MovingPlatformComponent begin --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const std::string& pathName) : Component(parent) {

}

void MovingPlatformComponent::LoadConfigData() {
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {
		AddMovingPlatform<SimpleMoverPlatformSubComponent>(NiPoint3::ZERO, false);
	}
	if (m_Parent->GetVar<bool>(u"platformIsMover")) {
		AddMovingPlatform<MoverPlatformSubComponent>();
	}
	if (m_Parent->GetVar<bool>(u"platformIsRotater")) {
		AddMovingPlatform<RotatorPlatformSubComponent>();
	}
	m_DirtyPathInfo = true;
}

void MovingPlatformComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	// For some reason we need to write this here instead of later on.
	outBitStream->Write(!m_Platforms.empty());

	outBitStream->Write(bIsInitialUpdate || m_DirtyPathInfo);
	if (bIsInitialUpdate || m_DirtyPathInfo) {
		outBitStream->Write(!m_PathName.empty());
		if (!m_PathName.empty()) {
			outBitStream->Write(static_cast<uint16_t>(m_PathName.size()));
			for (const auto& c : m_PathName) {
				outBitStream->Write(static_cast<uint16_t>(c));
			}
			outBitStream->Write(m_StartingWaypointIndex);
			outBitStream->Write(m_StartsIsInReverse);
		}
		if (!bIsInitialUpdate) m_DirtyPathInfo = false;
	}
	if (m_Platforms.empty()) return;

	for (const auto& platform : m_Platforms) {
		outBitStream->Write1(); // Has platform to write
		outBitStream->Write(platform->GetPlatformType());
		platform->Serialize(outBitStream, bIsInitialUpdate);
	}
	outBitStream->Write0(); // No more platforms to write
}

void MovingPlatformComponent::OnRebuildInitilized() {
	StopPathing();
}

void MovingPlatformComponent::OnCompleteRebuild() {
	if (m_NoAutoStart) return;

	StartPathing();
}

void MovingPlatformComponent::SetMovementState(eMovementPlatformState value) {
	// auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// subComponent->mState = value;

	// Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	// auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// subComponent->mDesiredWaypointIndex = index;
	// subComponent->mNextWaypointIndex = index;
	// subComponent->mShouldStopAtDesiredWaypoint = stopAtWaypoint;

	// StartPathing();
}

void MovingPlatformComponent::StartPathing() {
	// //GameMessages::SendStartPathing(m_Parent);
	// m_PathingStopped = false;

	// auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// subComponent->mShouldStopAtDesiredWaypoint = true;
	// subComponent->mState = eMovementPlatformState::Stationary;

	// NiPoint3 targetPosition;

	// if (m_Path != nullptr) {
	// 	const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
	// 	const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

	// 	subComponent->mPosition = currentWaypoint.position;
	// 	subComponent->mSpeed = currentWaypoint.movingPlatform.speed;
	// 	subComponent->mWaitTime = currentWaypoint.movingPlatform.wait;

	// 	targetPosition = nextWaypoint.position;
	// } else {
	// 	subComponent->mPosition = m_Parent->GetPosition();
	// 	subComponent->mSpeed = 1.0f;
	// 	subComponent->mWaitTime = 2.0f;

	// 	targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);
	// }

	// m_Parent->AddCallbackTimer(subComponent->mWaitTime, [this] {
	// 	SetMovementState(eMovementPlatformState::Moving);
	// 	});

	// const auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5f;

	// const auto travelNext = subComponent->mWaitTime + travelTime;

	// m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
	// 	for (CppScripts::Script* script : CppScripts::GetEntityScripts(m_Parent)) {
	// 		script->OnWaypointReached(m_Parent, subComponent->mNextWaypointIndex);
	// 	}
	// 	});

	// m_Parent->AddCallbackTimer(travelNext, [this] {
	// 	ContinuePathing();
	// 	});

	// //GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);

	// Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::ContinuePathing() {
	// auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// subComponent->mState = eMovementPlatformState::Stationary;

	// subComponent->mCurrentWaypointIndex = subComponent->mNextWaypointIndex;

	// NiPoint3 targetPosition;
	// uint32_t pathSize;
	// PathBehavior behavior;

	// if (m_Path != nullptr) {
	// 	const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
	// 	const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

	// 	subComponent->mPosition = currentWaypoint.position;
	// 	subComponent->mSpeed = currentWaypoint.movingPlatform.speed;
	// 	subComponent->mWaitTime = currentWaypoint.movingPlatform.wait; // + 2;

	// 	pathSize = m_Path->pathWaypoints.size() - 1;

	// 	behavior = static_cast<PathBehavior>(m_Path->pathBehavior);

	// 	targetPosition = nextWaypoint.position;
	// } else {
	// 	subComponent->mPosition = m_Parent->GetPosition();
	// 	subComponent->mSpeed = 1.0f;
	// 	subComponent->mWaitTime = 2.0f;

	// 	targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);

	// 	pathSize = 1;
	// 	behavior = PathBehavior::Loop;
	// }

	// if (m_Parent->GetLOT() == 9483) {
	// 	behavior = PathBehavior::Bounce;
	// } else {
	// 	return;
	// }

	// if (subComponent->mCurrentWaypointIndex >= pathSize) {
	// 	subComponent->mCurrentWaypointIndex = pathSize;
	// 	switch (behavior) {
	// 	case PathBehavior::Once:
	// 		Game::entityManager->SerializeEntity(m_Parent);
	// 		return;

	// 	case PathBehavior::Bounce:
	// 		subComponent->mInReverse = true;
	// 		break;

	// 	case PathBehavior::Loop:
	// 		subComponent->mNextWaypointIndex = 0;
	// 		break;

	// 	default:
	// 		break;
	// 	}
	// } else if (subComponent->mCurrentWaypointIndex == 0) {
	// 	subComponent->mInReverse = false;
	// }

	// if (subComponent->mInReverse) {
	// 	subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex - 1;
	// } else {
	// 	subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex + 1;
	// }

	// /*
	// subComponent->mNextWaypointIndex = 0;
	// subComponent->mCurrentWaypointIndex = 1;
	// */

	// //GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);

	// if (subComponent->mCurrentWaypointIndex == subComponent->mDesiredWaypointIndex) {
	// 	// TODO: Send event?
	// 	StopPathing();

	// 	return;
	// }

	// m_Parent->CancelCallbackTimers();

	// m_Parent->AddCallbackTimer(subComponent->mWaitTime, [this] {
	// 	SetMovementState(eMovementPlatformState::Moving);
	// 	});

	// auto travelTime = Vector3::Distance(targetPosition, subComponent->mPosition) / subComponent->mSpeed + 1.5;

	// if (m_Parent->GetLOT() == 9483) {
	// 	travelTime += 20;
	// }

	// const auto travelNext = subComponent->mWaitTime + travelTime;

	// m_Parent->AddCallbackTimer(travelTime, [subComponent, this] {
	// 	for (CppScripts::Script* script : CppScripts::GetEntityScripts(m_Parent)) {
	// 		script->OnWaypointReached(m_Parent, subComponent->mNextWaypointIndex);
	// 	}
	// 	});

	// m_Parent->AddCallbackTimer(travelNext, [this] {
	// 	ContinuePathing();
	// 	});

	// Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StopPathing() {
	//m_Parent->CancelCallbackTimers();

	// auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// m_PathingStopped = true;

	// subComponent->mState = eMovementPlatformState::Stopped;
	// subComponent->mDesiredWaypointIndex = -1;
	// subComponent->mShouldStopAtDesiredWaypoint = false;

	// Game::entityManager->SerializeEntity(m_Parent);

	//GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);
}

void MovingPlatformComponent::SetSerialized(bool value) {
	// m_Serialize = value;
}

bool MovingPlatformComponent::GetNoAutoStart() const {
	return false;
	// return m_NoAutoStart;
}

void MovingPlatformComponent::SetNoAutoStart(const bool value) {
	// m_NoAutoStart = value;
}

void MovingPlatformComponent::WarpToWaypoint(size_t index) {
	// const auto& waypoint = m_Path->pathWaypoints[index];

	// m_Parent->SetPosition(waypoint.position);
	// m_Parent->SetRotation(waypoint.rotation);

	// Game::entityManager->SerializeEntity(m_Parent);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	return 0;
	// return m_Path->pathWaypoints.size() - 1;
}

//------------- MovingPlatformComponent end --------------
