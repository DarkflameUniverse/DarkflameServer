/*
 * Darkflame Universe
 * Copyright 2019
 */

#include "MovingPlatformComponent.h"
#include "PlatformSubComponent.h"
#include "MoverSubComponent.h"
#include "SimpleMoverSubComponent.h"
#include "RotatorSubComponent.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "GameMessages.h"
#include "SimplePhysicsComponent.h"
#include "Zone.h"
#include "eMovementPlatformState.h"

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName)
	: Component(parent, componentID) {

	m_MoverSubComponentType = eMoverSubComponentType::mover;
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);
	m_NoAutoStart = false;

	if (m_Path == nullptr && !pathName.empty()) {
		LOG("Path not found: %s", pathName.c_str());
	}

	SetupPlatformSubComponents();
}

MovingPlatformComponent::~MovingPlatformComponent() = default;

void MovingPlatformComponent::SetupPlatformSubComponents() {
	// Read component properties matching client SetupPlatform
	bool isMover = m_Parent->GetVar<bool>(u"platformIsMover");
	bool isSimpleMover = m_Parent->GetVar<bool>(u"platformIsSimpleMover");
	bool isRotater = m_Parent->GetVar<bool>(u"platformIsRotater");

	// Read sound GUIDs
	m_PlatformSoundStart = m_Parent->GetVarAsString(u"platformSoundStart");
	m_PlatformSoundTravel = m_Parent->GetVarAsString(u"platformSoundTravel");
	m_PlatformSoundStop = m_Parent->GetVarAsString(u"platformSoundStop");

	// If no flags set but we have a path, default to mover (backwards compatibility)
	if (!isMover && !isSimpleMover && !isRotater) {
		if (m_Path && m_Path->pathType == PathType::MovingPlatform) {
			isMover = true;
		}
	}

	// Create mover subcomponent
	if (isMover && m_Path) {
		m_MoverSubComponentType = eMoverSubComponentType::mover;
		auto mover = std::make_unique<MoverSubComponent>(m_Parent, m_Path);

		if (!m_PathName.empty()) {
			bool reverse = m_Parent->GetVar<bool>(u"reverse");
			int32_t startPoint = m_Parent->GetVarAs<int32_t>(u"startPoint");
			mover->SetInReverse(reverse);
			if (startPoint >= 0 && startPoint < static_cast<int32_t>(m_Path->pathWaypoints.size())) {
				mover->SetupWaypointSegment(static_cast<uint32_t>(startPoint));
			}
			mover->SetActive(true);
		}

		m_MoverSubComponent = std::move(mover);
	}

	// Create simple mover subcomponent
	if (isSimpleMover) {
		m_MoverSubComponentType = eMoverSubComponentType::simpleMover;

		NiPoint3 platformMove{};
		platformMove.x = m_Parent->GetVar<float>(u"platformMoveX");
		platformMove.y = m_Parent->GetVar<float>(u"platformMoveY");
		platformMove.z = m_Parent->GetVar<float>(u"platformMoveZ");

		float platformMoveTime = m_Parent->GetVar<float>(u"platformMoveTime");

		NiPoint3 startPos = m_Parent->GetDefaultPosition();
		NiQuaternion startRot = m_Parent->GetDefaultRotation();

		m_MoverSubComponent = std::make_unique<SimpleMoverSubComponent>(
			m_Parent, startPos, startRot, platformMove, platformMoveTime);
	}

	// Create rotator subcomponent (can coexist with mover)
	if (isRotater && m_Path) {
		auto rotator = std::make_unique<RotatorSubComponent>(m_Parent, m_Path);

		if (!m_PathName.empty()) {
			bool reverse = m_Parent->GetVar<bool>(u"reverse");
			int32_t startPoint = m_Parent->GetVarAs<int32_t>(u"startPoint");
			rotator->SetInReverse(reverse);
			if (startPoint >= 0 && startPoint < static_cast<int32_t>(m_Path->pathWaypoints.size())) {
				rotator->SetupWaypointSegment(static_cast<uint32_t>(startPoint));
			}
			rotator->SetActive(true);
		}

		m_RotatorSubComponent = std::move(rotator);
	}

	// Fallback: if nothing was created, create a default mover
	if (!m_MoverSubComponent && !m_RotatorSubComponent) {
		m_MoverSubComponentType = eMoverSubComponentType::mover;
		m_MoverSubComponent = std::make_unique<MoverSubComponent>(m_Parent, m_Path);
	}
}

void MovingPlatformComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (!m_Serialize) {
		outBitStream.Write<bool>(false);
		outBitStream.Write<bool>(false);
		return;
	}

	outBitStream.Write<bool>(true);

	auto hasPath = !m_PathingStopped && !m_PathName.empty();
	outBitStream.Write(hasPath);

	if (hasPath) {
		outBitStream.Write1();

		outBitStream.Write<uint16_t>(m_PathName.size());
		for (const auto& c : m_PathName) {
			outBitStream.Write<uint16_t>(c);
		}

		outBitStream.Write<uint32_t>(m_MoverSubComponent ? m_MoverSubComponent->GetCurrentWaypointIndex() : 0);
		outBitStream.Write<bool>(m_MoverSubComponent ? m_MoverSubComponent->GetInReverse() : false);
	}

	const auto hasPlatform = m_MoverSubComponent != nullptr;
	outBitStream.Write<bool>(hasPlatform);

	if (hasPlatform) {
		outBitStream.Write(m_MoverSubComponentType);
		m_MoverSubComponent->Serialize(outBitStream, bIsInitialUpdate);
	}
}

void MovingPlatformComponent::Update(float deltaTime) {
	if (!m_Serialize) return;

	// Track whether we were travelling before update for sound management
	bool wasTravelling = m_MoverSubComponent &&
		(m_MoverSubComponent->GetState() & PlatformState::Travelling);

	bool dirty = false;

	if (m_MoverSubComponent) {
		m_MoverSubComponent->Update(deltaTime, dirty);
	}

	if (m_RotatorSubComponent) {
		m_RotatorSubComponent->Update(deltaTime, dirty);
	}

	// Handle travel sound looping (matching client PlayTravelSound/StopTravelSound)
	if (m_MoverSubComponent && !m_PlatformSoundTravel.empty()) {
		bool isTravelling = m_MoverSubComponent->GetState() & PlatformState::Travelling;
		if (isTravelling && !wasTravelling) {
			// Started travelling — play looping travel sound
			GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, m_PlatformSoundTravel);
		}
		// Note: the client stops the travel sound on arrival/stop via StopTravelSound.
		// SendPlayNDAudioEmitter doesn't support stopping, so the sound will naturally end
		// or be replaced by the arrive/stop sound.
	}

	if (dirty) {
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void MovingPlatformComponent::OnQuickBuildInitilized() {
	StopPathing();
}

void MovingPlatformComponent::OnCompleteQuickBuild() {
	if (m_NoAutoStart) return;
	StartPathing();
}

void MovingPlatformComponent::SetMovementState(uint32_t state) {
	if (m_MoverSubComponent) m_MoverSubComponent->SetState(state);
	if (m_RotatorSubComponent) m_RotatorSubComponent->SetState(state);
	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	m_PathingStopped = false;

	if (m_MoverSubComponent) m_MoverSubComponent->GotoWaypoint(index, stopAtWaypoint);
	if (m_RotatorSubComponent) m_RotatorSubComponent->GotoWaypoint(index, stopAtWaypoint);

	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StartPathing() {
	m_PathingStopped = false;

	if (m_MoverSubComponent) m_MoverSubComponent->StartPathing();
	if (m_RotatorSubComponent) m_RotatorSubComponent->StartPathing();

	if (m_MoverSubComponent) {
		GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS,
			m_MoverSubComponent->GetShouldStopAtDesiredWaypoint(),
			m_MoverSubComponent->GetCurrentWaypointIndex(),
			m_MoverSubComponent->GetDesiredWaypointIndex(),
			m_MoverSubComponent->GetNextWaypointIndex(),
			static_cast<eMovementPlatformState>(m_MoverSubComponent->GetSerializedState()),
			m_MoverSubComponent->GetInReverse(),
			m_MoverSubComponent->GetIdleTimeElapsed(),
			m_MoverSubComponent->GetMoveTimeElapsed(),
			m_MoverSubComponent->GetPercentBetweenPoints(),
			m_MoverSubComponent->GetPosition());
	}

	if (!m_PlatformSoundStart.empty()) {
		GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, m_PlatformSoundStart);
	}

	Game::entityManager->SerializeEntity(m_Parent);
}

void MovingPlatformComponent::StopPathing() {
	m_PathingStopped = true;

	if (m_MoverSubComponent) m_MoverSubComponent->StopPathing();
	if (m_RotatorSubComponent) m_RotatorSubComponent->StopPathing();

	GameMessages::SendPlatformResync(m_Parent, UNASSIGNED_SYSTEM_ADDRESS,
		false, 0, -1, 0, eMovementPlatformState::Stopped);

	if (!m_PlatformSoundStop.empty()) {
		GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, m_PlatformSoundStop);
	}

	Game::entityManager->SerializeEntity(m_Parent);
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
	if (m_MoverSubComponent) m_MoverSubComponent->WarpToWaypoint(index);
	if (m_RotatorSubComponent) m_RotatorSubComponent->WarpToWaypoint(index);
	Game::entityManager->SerializeEntity(m_Parent);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	if (m_MoverSubComponent) return m_MoverSubComponent->GetLastWaypointIndex();
	if (m_RotatorSubComponent) return m_RotatorSubComponent->GetLastWaypointIndex();
	return 0;
}

PlatformSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	if (m_MoverSubComponent) return m_MoverSubComponent.get();
	return nullptr;
}
