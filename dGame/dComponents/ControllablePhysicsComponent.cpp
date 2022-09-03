#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "dLogger.h"
#include "Game.h"

#include "dpWorld.h"
#include "dpEntity.h"
#include "CDPhysicsComponentTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDClientManager.h"
#include "EntityManager.h"
#include "Character.h"
#include "dZoneManager.h"

ControllablePhysicsComponent::ControllablePhysicsComponent(Entity* entity) : Component(entity) {
	m_Position = {};
	m_Rotation = NiQuaternion::IDENTITY;
	m_Velocity = {};
	m_AngularVelocity = {};
	m_InJetpackMode = false;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;
	m_DirtyAngularVelocity = true;
	m_dpEntity = nullptr;
	m_Static = false;
	m_SpeedMultiplier = 1;
	m_GravityScale = 1;
	m_DirtyCheats = false;
	m_IgnoreMultipliers = false;
	m_PickupRadius = 0.0f;
	m_DirtyPickupRadiusScale = true;
	m_IsTeleporting = false;

	if (entity->GetLOT() != 1) // Other physics entities we care about will be added by BaseCombatAI
		return;

	if (entity->GetLOT() == 1) {
		Game::logger->Log("ControllablePhysicsComponent", "Using patch to load minifig physics");

		float radius = 1.5f;
		m_dpEntity = new dpEntity(m_Parent->GetObjectID(), radius, false);
		m_dpEntity->SetCollisionGroup(COLLISION_GROUP_DYNAMIC | COLLISION_GROUP_FRIENDLY);
		dpWorld::Instance().AddEntity(m_dpEntity);
	}
}

ControllablePhysicsComponent::~ControllablePhysicsComponent() {
	if (m_dpEntity) {
		dpWorld::Instance().RemoveEntity(m_dpEntity);
	}
}

void ControllablePhysicsComponent::Update(float deltaTime) {

}

void ControllablePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	//If this is a creation, then we assume the position is dirty, even when it isn't.
	//This is because new clients will still need to receive the position.
	//if (bIsInitialUpdate) m_DirtyPosition = true;

	if (bIsInitialUpdate) {
		outBitStream->Write(m_InJetpackMode);
		if (m_InJetpackMode) {
			outBitStream->Write(m_JetpackEffectID);
			outBitStream->Write(m_JetpackFlying);
			outBitStream->Write(m_JetpackBypassChecks);
		}

		outBitStream->Write0(); //This contains info about immunities, but for now I'm leaving it out.
	}

	if (m_SpeedMultiplier < 1.0f) {
		m_DirtyCheats = false;
	}

	if (m_IgnoreMultipliers) {
		m_DirtyCheats = false;
	}

	outBitStream->Write(m_DirtyCheats);
	if (m_DirtyCheats) {
		outBitStream->Write(m_GravityScale);
		outBitStream->Write(m_SpeedMultiplier);

		m_DirtyCheats = false;
	}

	outBitStream->Write(m_DirtyPickupRadiusScale);
	if (m_DirtyPickupRadiusScale) {
		outBitStream->Write(m_PickupRadius);
		outBitStream->Write0(); //No clue what this is so im leaving it false.
		m_DirtyPickupRadiusScale = false;
	}

	outBitStream->Write0();

	outBitStream->Write(m_DirtyPosition || bIsInitialUpdate);
	if (m_DirtyPosition || bIsInitialUpdate) {
		outBitStream->Write(m_Position.x);
		outBitStream->Write(m_Position.y);
		outBitStream->Write(m_Position.z);

		outBitStream->Write(m_Rotation.x);
		outBitStream->Write(m_Rotation.y);
		outBitStream->Write(m_Rotation.z);
		outBitStream->Write(m_Rotation.w);

		outBitStream->Write(m_IsOnGround);
		outBitStream->Write(m_IsOnRail);

		outBitStream->Write(m_DirtyVelocity);
		if (m_DirtyVelocity) {
			outBitStream->Write(m_Velocity.x);
			outBitStream->Write(m_Velocity.y);
			outBitStream->Write(m_Velocity.z);
		}

		outBitStream->Write(m_DirtyAngularVelocity);
		if (m_DirtyAngularVelocity) {
			outBitStream->Write(m_AngularVelocity.x);
			outBitStream->Write(m_AngularVelocity.y);
			outBitStream->Write(m_AngularVelocity.z);
		}

		outBitStream->Write0();
	}

	if (!bIsInitialUpdate) {
		outBitStream->Write(m_IsTeleporting);
		m_IsTeleporting = false;
	}
}

void ControllablePhysicsComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("ControllablePhysicsComponent", "Failed to find char tag!");
		return;
	}

	m_Parent->GetCharacter()->LoadXmlRespawnCheckpoints();

	character->QueryAttribute("lzx", &m_Position.x);
	character->QueryAttribute("lzy", &m_Position.y);
	character->QueryAttribute("lzz", &m_Position.z);
	character->QueryAttribute("lzrx", &m_Rotation.x);
	character->QueryAttribute("lzry", &m_Rotation.y);
	character->QueryAttribute("lzrz", &m_Rotation.z);
	character->QueryAttribute("lzrw", &m_Rotation.w);

	m_DirtyPosition = true;
}

void ControllablePhysicsComponent::ResetFlags() {
	m_DirtyAngularVelocity = false;
	m_DirtyPosition = false;
	m_DirtyVelocity = false;
}

void ControllablePhysicsComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("ControllablePhysicsComponent", "Failed to find char tag while updating XML!");
		return;
	}

	auto zoneInfo = dZoneManager::Instance()->GetZone()->GetZoneID();

	if (zoneInfo.GetMapID() != 0 && zoneInfo.GetCloneID() == 0) {
		character->SetAttribute("lzx", m_Position.x);
		character->SetAttribute("lzy", m_Position.y);
		character->SetAttribute("lzz", m_Position.z);
		character->SetAttribute("lzrx", m_Rotation.x);
		character->SetAttribute("lzry", m_Rotation.y);
		character->SetAttribute("lzrz", m_Rotation.z);
		character->SetAttribute("lzrw", m_Rotation.w);
	}
}

void ControllablePhysicsComponent::SetPosition(const NiPoint3& pos) {
	if (m_Static) {
		return;
	}

	m_Position.x = pos.x;
	m_Position.y = pos.y;
	m_Position.z = pos.z;
	m_DirtyPosition = true;

	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void ControllablePhysicsComponent::SetRotation(const NiQuaternion& rot) {
	if (m_Static) {
		return;
	}

	m_Rotation = rot;
	m_DirtyPosition = true;

	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}

void ControllablePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	if (m_Static) {
		return;
	}

	m_Velocity = vel;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;

	if (m_dpEntity) m_dpEntity->SetVelocity(vel);
}

void ControllablePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	if (m_Static) {
		return;
	}

	m_AngularVelocity = vel;
	m_DirtyPosition = true;
	m_DirtyAngularVelocity = true;
}

void ControllablePhysicsComponent::SetIsOnGround(bool val) {
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void ControllablePhysicsComponent::SetIsOnRail(bool val) {
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void ControllablePhysicsComponent::SetDirtyPosition(bool val) {
	m_DirtyPosition = val;
}

void ControllablePhysicsComponent::SetDirtyVelocity(bool val) {
	m_DirtyVelocity = val;
}

void ControllablePhysicsComponent::SetDirtyAngularVelocity(bool val) {
	m_DirtyAngularVelocity = val;
}

void ControllablePhysicsComponent::AddPickupRadiusScale(float value) {
	m_ActivePickupRadiusScales.push_back(value);
	if (value > m_PickupRadius) {
		m_PickupRadius = value;
		m_DirtyPickupRadiusScale = true;
	}
}

void ControllablePhysicsComponent::RemovePickupRadiusScale(float value) {
	// Attempt to remove pickup radius from active radii
	const auto pos = std::find(m_ActivePickupRadiusScales.begin(), m_ActivePickupRadiusScales.end(), value);
	if (pos != m_ActivePickupRadiusScales.end()) {
		m_ActivePickupRadiusScales.erase(pos);
	} else {
		Game::logger->Log("ControllablePhysicsComponent", "Warning: Could not find pickup radius %f in list of active radii.  List has %i active radii.", value, m_ActivePickupRadiusScales.size());
		return;
	}

	// Recalculate pickup radius since we removed one by now
	m_PickupRadius = 0.0f;
	m_DirtyPickupRadiusScale = true;
	for (uint32_t i = 0; i < m_ActivePickupRadiusScales.size(); i++) {
		auto candidateRadius = m_ActivePickupRadiusScales[i];
		if (m_PickupRadius < candidateRadius) m_PickupRadius = candidateRadius;
	}
	EntityManager::Instance()->SerializeEntity(m_Parent);
}
