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
#include "CDClientManager.h"
#include "CDMovingPlatformComponentTable.h"
#include "Zone.h"

 //------------- PlatformSubComponent begin --------------

PlatformSubComponent::PlatformSubComponent(MovingPlatformComponent* parentComponent) {
	DluAssert(parentComponent != nullptr);
	m_ParentComponent = parentComponent;
	m_Position = parentComponent->GetParent()->GetPosition();
	m_Rotation = parentComponent->GetParent()->GetRotation();

	m_State = eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint;
	m_DesiredWaypointIndex = -1;
	m_PercentUntilNextWaypoint = 0.0f;
	m_CurrentWaypointIndex = 0;
	m_NextWaypointIndex = -1;
	m_IdleTimeElapsed = 0.0f;
	m_Speed = 0.0f;
	m_WaitTime = 0.0f;
	m_MoveTimeElapsed = 0.0f;
	m_IsDirty = false;
	m_InReverse = false;
	m_ShouldStopAtDesiredWaypoint = false;
	m_LinearVelocity = NiPoint3Constant::ZERO;
	m_AngularVelocity = NiPoint3Constant::ZERO;
	m_TimeBasedMovement = false;
	m_Path = nullptr; 
}

void PlatformSubComponent::Update(float deltaTime) {
	if (m_State == 0 || !m_Path) return;
	if (m_State & eMovementPlatformState::Travelling) {
		m_MoveTimeElapsed += deltaTime;

		// Only need to recalculate the linear velocity if the speed is changing between waypoints
		// Unfortunately for the poor client, they chose to, instead of change the speed once at the start of the waypoint,
		// the speed is changed over the course of the waypoint. This means we have to recalculate the linear velocity every frame.
		// yay.
		if (m_Speed == 0.0f || (GetCurrentWaypoint().movingPlatform.speed != GetNextWaypoint().movingPlatform.speed)) {
			UpdateLinearVelocity();
			m_IsDirty = true;
		}
		m_Position += m_LinearVelocity * deltaTime;
		if (CalculatePercentToNextWaypoint() > 0.99) {
			m_MoveTimeElapsed = 0;
			m_ParentComponent->GetParent()->SetPosition(m_Position);
			m_InReverse ? AdvanceToNextReverseWaypoint() : AdvanceToNextWaypoint();
			m_IsDirty = true;
			Game::entityManager->SerializeEntity(m_ParentComponent->GetParent());
		}
	}
}

float PlatformSubComponent::CalculatePercentToNextWaypoint() {
	if (m_TimeBasedMovement) return 0;
	float distanceToNextWaypoint = (GetNextWaypoint().position - GetCurrentWaypoint().position).Length();
	if (distanceToNextWaypoint == 0.0f) return 0;
	float distanceToCurrentWaypoint = (m_Position - GetCurrentWaypoint().position).Length();
	return distanceToCurrentWaypoint / distanceToNextWaypoint;
}

void PlatformSubComponent::UpdateAngularVelocity() {
	// Update the angular velocity
	// This one is sure to be fun...
}

void PlatformSubComponent::UpdateLinearVelocity() {
	m_LinearVelocity = CalculateLinearVelocity();
}

void PlatformSubComponent::AdvanceToNextWaypoint() {
	uint32_t numWaypoints = m_Path->pathWaypoints.size();
	m_CurrentWaypointIndex = m_NextWaypointIndex;
	m_ParentComponent->GetParent()->SetPosition(GetCurrentWaypoint().position);
	m_ParentComponent->GetParent()->SetRotation(GetCurrentWaypoint().rotation);
	uint32_t nextWaypointIndex = m_CurrentWaypointIndex + 1;
	if (numWaypoints <= nextWaypointIndex) {
		PathBehavior behavior = m_Path->pathBehavior;
		if (behavior == PathBehavior::Once) {
			nextWaypointIndex = m_Path->pathWaypoints.size() - 1;
		} else if (behavior == PathBehavior::Bounce) {
			nextWaypointIndex = m_Path->pathWaypoints.size() - 2;
			m_InReverse = true;
		} else {
			m_NextWaypointIndex = 0;
		}
	}
	m_NextWaypointIndex = nextWaypointIndex;
	m_DesiredWaypointIndex = nextWaypointIndex;
	UpdateLinearVelocity();
	UpdateAngularVelocity();
	m_IsDirty = true;
}

void PlatformSubComponent::AdvanceToNextReverseWaypoint() {
	uint32_t numWaypoints = m_Path->pathWaypoints.size();
	m_ParentComponent->GetParent()->SetPosition(GetCurrentWaypoint().position);
	m_ParentComponent->GetParent()->SetRotation(GetCurrentWaypoint().rotation);
	m_CurrentWaypointIndex = m_NextWaypointIndex;
	int32_t nextWaypointIndex = m_CurrentWaypointIndex - 1;
	if (nextWaypointIndex < 0) {
		PathBehavior behavior = m_Path->pathBehavior;
		if (behavior == PathBehavior::Once) {
			nextWaypointIndex = 0;
		} else if (behavior == PathBehavior::Bounce) {
			nextWaypointIndex = 1;
			m_InReverse = false;
		} else {
			nextWaypointIndex = m_Path->pathWaypoints.size() - 1;
		}
	}
	m_NextWaypointIndex = nextWaypointIndex;
	m_DesiredWaypointIndex = nextWaypointIndex;
	UpdateLinearVelocity();
	UpdateAngularVelocity();
	m_IsDirty = true;
}

void PlatformSubComponent::SetupPath(const std::string& pathName, uint32_t startingWaypointIndex, bool startsInReverse) {
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);
	LOG("setting up path %s", pathName.c_str());
	if (!m_Path) {
		LOG("Failed to find path (%s)", pathName.c_str());
		return;
	}
	m_InReverse = startsInReverse;
	m_CurrentWaypointIndex = startingWaypointIndex;
	m_TimeBasedMovement = m_Path->movingPlatform.timeBasedMovement;
	m_NextWaypointIndex = m_InReverse ? m_CurrentWaypointIndex - 1 : m_CurrentWaypointIndex + 1;
}

const PathWaypoint& PlatformSubComponent::GetNextWaypoint() const {
	DluAssert(m_Path != nullptr);
	return m_Path->pathWaypoints.at(m_NextWaypointIndex);
}

const PathWaypoint& PlatformSubComponent::GetCurrentWaypoint() const {
	DluAssert(m_Path != nullptr);
	return m_Path->pathWaypoints.at(m_CurrentWaypointIndex);
}

float PlatformSubComponent::CalculateSpeed() const {
	float speed;
	if (m_TimeBasedMovement) {
		float unitizedDirection = 1.0f / (GetNextWaypoint().position - GetCurrentWaypoint().position).Length();
		speed = unitizedDirection / GetCurrentWaypoint().movingPlatform.speed;
	} else {
		LOG("%i %i", m_CurrentWaypointIndex, m_NextWaypointIndex); 
		Game::logger->Flush();
		speed = (GetNextWaypoint().movingPlatform.speed - GetCurrentWaypoint().movingPlatform.speed) * m_PercentUntilNextWaypoint + GetCurrentWaypoint().movingPlatform.speed;
	}
	return speed;
}

NiPoint3 PlatformSubComponent::CalculateLinearVelocity() {
	return (GetNextWaypoint().position - GetCurrentWaypoint().position).Unitize() * CalculateSpeed();
}

void PlatformSubComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(bIsInitialUpdate || m_IsDirty);
	if (!(bIsInitialUpdate || m_IsDirty)) return;
	outBitStream->Write(m_State);
	outBitStream->Write(m_DesiredWaypointIndex);
	outBitStream->Write(m_ShouldStopAtDesiredWaypoint);
	outBitStream->Write(m_InReverse);
	outBitStream->Write(m_PercentUntilNextWaypoint);
	outBitStream->Write(m_Position.x);
	outBitStream->Write(m_Position.y);
	outBitStream->Write(m_Position.z);
	outBitStream->Write(m_CurrentWaypointIndex);
	outBitStream->Write(m_NextWaypointIndex);
	outBitStream->Write(m_IdleTimeElapsed);
	outBitStream->Write(m_MoveTimeElapsed);

	if (!bIsInitialUpdate) m_IsDirty = false;
}

void PlatformSubComponent::StartPathing() {
	m_State |= eMovementPlatformState::Travelling;
	m_State &= ~eMovementPlatformState::Stopped;
	m_State &= ~eMovementPlatformState::Waiting;
	m_IsDirty = true;
	m_CurrentWaypointIndex = m_ParentComponent->GetStartingWaypointIndex();
	m_InReverse = m_ParentComponent->GetStartsIsInReverse();
	m_NextWaypointIndex = m_InReverse ? m_CurrentWaypointIndex - 1 : m_CurrentWaypointIndex + 1;
	Game::entityManager->SerializeEntity(m_ParentComponent->GetParent());
}

void PlatformSubComponent::ResumePathing() {
	if (m_State & eMovementPlatformState::Stopped && (m_State & eMovementPlatformState::ReachedDesiredWaypoint) == 0) {
		StartPathing();
	}
	if (m_State & eMovementPlatformState::Travelling == 0) {
		m_State |= eMovementPlatformState::Waiting;
		m_State &= ~eMovementPlatformState::Stopped;
		m_State &= ~eMovementPlatformState::Travelling;
		m_IsDirty = true;
	} else {
		m_State &= eMovementPlatformState::Waiting;
		m_State &= eMovementPlatformState::Travelling;
		m_State &= eMovementPlatformState::Stopped;
		m_IsDirty = true;
		UpdateLinearVelocity();
		UpdateAngularVelocity();
	}
}

void PlatformSubComponent::StopPathing() {
	m_State |= eMovementPlatformState::Stopped;
	m_State &= ~eMovementPlatformState::Travelling;
	m_State &= ~eMovementPlatformState::Waiting;
	m_LinearVelocity = NiPoint3Constant::ZERO;
	m_AngularVelocity = NiPoint3Constant::ZERO;
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
	m_StartAtEnd = m_ParentComponent->GetParent()->GetVar<bool>(u"platformStartAtEnd");
}

void SimpleMoverPlatformSubComponent::LoadDataFromTemplate() {
	if (!m_ParentComponent->GetParent()->GetVar<bool>(u"dbonly")) return;

	auto* movingPlatformTable = CDClientManager::GetTable<CDMovingPlatformComponentTable>();
	if (movingPlatformTable == nullptr) return;

	const auto& platformEntry = movingPlatformTable->GetPlatformEntry(m_ParentComponent->GetComponentId());
	if (!platformEntry || !platformEntry->platformIsSimpleMover) return;

	NiPoint3 platformMove = platformEntry->platformMove;
	float moveTime = platformEntry->moveTime;
	m_PlatformMove = platformMove;
	m_MoveTime = moveTime;
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
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
}

void MovingPlatformComponent::LoadDataFromTemplate() {
	std::for_each(m_Platforms.begin(), m_Platforms.end(), [](const std::unique_ptr<PlatformSubComponent>& platform) { platform->LoadDataFromTemplate(); });
}

void MovingPlatformComponent::LoadConfigData() {
	if (m_Parent->GetVar<bool>(u"platformIsMover")) {
		AddMovingPlatform<MoverPlatformSubComponent>();
	}
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {
		AddMovingPlatform<SimpleMoverPlatformSubComponent>(NiPoint3Constant::ZERO, false);
	}
	if (m_Parent->GetVar<bool>(u"platformIsRotater")) {
		AddMovingPlatform<RotatorPlatformSubComponent>();
	}
	m_StartingWaypointIndex = m_Parent->GetVar<uint32_t>(u"attached_path_start");
	m_StartsIsInReverse = false;
	m_DirtyPathInfo = true;
	m_StartOnload = m_Parent->GetVar<bool>(u"startPathingOnLoad");
}

void MovingPlatformComponent::Update(float deltaTime) {
	std::for_each(m_Platforms.begin(), m_Platforms.end(), [deltaTime](const std::unique_ptr<PlatformSubComponent>& platform) { platform->Update(deltaTime); });
}

void MovingPlatformComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
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

void MovingPlatformComponent::OnQuickBuildInitilized() {
	StopPathing();
}

void MovingPlatformComponent::OnCompleteQuickBuild() {
	if (m_NoAutoStart) return;

	StartPathing();
}

void MovingPlatformComponent::SetMovementState(eMovementPlatformState value) {
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
}

void MovingPlatformComponent::StartPathing() {
	std::for_each(m_Platforms.begin(), m_Platforms.end(), [](const std::unique_ptr<PlatformSubComponent>& platform) {
		platform->StartPathing();
		});
}

void MovingPlatformComponent::ContinuePathing() {

}

void MovingPlatformComponent::StopPathing() {

}

bool MovingPlatformComponent::GetNoAutoStart() const {
	return false;
}

void MovingPlatformComponent::SetNoAutoStart(const bool value) {

}

void MovingPlatformComponent::WarpToWaypoint(size_t index) {

}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	return 0;
}

//------------- MovingPlatformComponent end --------------
