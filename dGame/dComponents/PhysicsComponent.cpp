#include "PhysicsComponent.h"

#include "eReplicaComponentType.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"

#include "dpEntity.h"
#include "dpWorld.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"

#include "EntityInfo.h"

PhysicsComponent::PhysicsComponent(Entity* parent) : Component(parent) {
	m_Position = NiPoint3Constant::ZERO;
	m_Rotation = NiQuaternionConstant::IDENTITY;
	m_DirtyPosition = false;
}

void PhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(bIsInitialUpdate || m_DirtyPosition);
	if (bIsInitialUpdate || m_DirtyPosition) {
		outBitStream.Write(m_Position.x);
		outBitStream.Write(m_Position.y);
		outBitStream.Write(m_Position.z);
		outBitStream.Write(m_Rotation.x);
		outBitStream.Write(m_Rotation.y);
		outBitStream.Write(m_Rotation.z);
		outBitStream.Write(m_Rotation.w);
		if (!bIsInitialUpdate) m_DirtyPosition = false;
	}
}

dpEntity* PhysicsComponent::CreatePhysicsEntity(eReplicaComponentType type) {
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	auto componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), type);

	CDPhysicsComponentTable* physComp = CDClientManager::GetTable<CDPhysicsComponentTable>();

	if (physComp == nullptr) return nullptr;

	auto* info = physComp->GetByID(componentID);
	if (info == nullptr || info->physicsAsset == "" || info->physicsAsset == "NO_PHYSICS") return nullptr;

	dpEntity* toReturn;
	if (info->physicsAsset == "miscellaneous\\misc_phys_10x1x5.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 10.0f, 5.0f, 1.0f);
	} else if (info->physicsAsset == "miscellaneous\\misc_phys_640x640.hkx") {
		// TODO Fix physics simulation to do simulation at high velocities due to bullet through paper problem...
		toReturn = new dpEntity(m_Parent->GetObjectID(), 1638.4f, 13.521004f * 2.0f, 1638.4f);

		// Move this down by 13.521004 units so it is still effectively at the same height as before
		m_Position = m_Position - NiPoint3Constant::UNIT_Y * 13.521004f;
	} else if (info->physicsAsset == "env\\trigger_wall_tall.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 10.0f, 25.0f, 1.0f);
	} else if (info->physicsAsset == "env\\env_gen_placeholderphysics.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 20.0f, 20.0f, 20.0f);
	} else if (info->physicsAsset == "env\\POI_trigger_wall.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 1.0f, 12.5f, 20.0f); // Not sure what the real size is
	} else if (info->physicsAsset == "env\\NG_NinjaGo\\env_ng_gen_gate_chamber_puzzle_ceiling_tile_falling_phantom.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 18.0f, 5.0f, 15.0f);
		m_Position += m_Rotation.GetForwardVector() * 7.5f;
	} else if (info->physicsAsset == "env\\NG_NinjaGo\\ng_flamejet_brick_phantom.HKX") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 1.0f, 1.0f, 12.0f);
		m_Position += m_Rotation.GetForwardVector() * 6.0f;
	} else if (info->physicsAsset == "env\\Ring_Trigger.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 6.0f, 6.0f, 6.0f);
	} else if (info->physicsAsset == "env\\vfx_propertyImaginationBall.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 4.5f);
	} else if (info->physicsAsset == "env\\env_won_fv_gas-blocking-volume.hkx") {
		toReturn = new dpEntity(m_Parent->GetObjectID(), 390.496826f, 111.467964f, 600.821534f, true);
		m_Position.y -= (111.467964f * m_Parent->GetDefaultScale()) / 2;
	} else {
		// LOG_DEBUG("This one is supposed to have %s", info->physicsAsset.c_str());

		//add fallback cube:
		toReturn = new dpEntity(m_Parent->GetObjectID(), 2.0f, 2.0f, 2.0f);
	}
	return toReturn;
}

dpEntity* PhysicsComponent::CreatePhysicsLnv(const float scale, const eReplicaComponentType type) const {
	int pcShapeType = -1;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float width = 0.0f; //aka "radius"
	float height = 0.0f;
	dpEntity* toReturn = nullptr;

	if (m_Parent->HasVar(u"primitiveModelType")) {
		pcShapeType = m_Parent->GetVar<int32_t>(u"primitiveModelType");
		x = m_Parent->GetVar<float>(u"primitiveModelValueX");
		y = m_Parent->GetVar<float>(u"primitiveModelValueY");
		z = m_Parent->GetVar<float>(u"primitiveModelValueZ");
	} else {
		CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
		auto componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), type);

		CDPhysicsComponentTable* physComp = CDClientManager::GetTable<CDPhysicsComponentTable>();

		if (physComp == nullptr) return nullptr;

		auto info = physComp->GetByID(componentID);

		if (info == nullptr) return nullptr;

		pcShapeType = info->pcShapeType;
		width = info->playerRadius;
		height = info->playerHeight;
	}

	switch (pcShapeType) {
	case 0: { // HKX type
		break;
	}
	case 1: { //Make a new box shape
		NiPoint3 boxSize(x, y, z);
		if (x == 0.0f) {
			//LU has some weird values, so I think it's best to scale them down a bit
			if (height < 0.5f) height = 2.0f;
			if (width < 0.5f) width = 2.0f;

			//Scale them:
			width = width * scale;
			height = height * scale;

			boxSize = NiPoint3(width, height, width);
		}

		toReturn = new dpEntity(m_Parent->GetObjectID(), boxSize);

		toReturn->SetPosition({ m_Position.x, m_Position.y - (height / 2), m_Position.z });
		break;
	}
	case 2: { //Make a new cylinder shape
		break;
	}
	case 3: { //Make a new sphere shape
		auto [x, y, z] = m_Position;
		toReturn = new dpEntity(m_Parent->GetObjectID(), width);
		toReturn->SetPosition({ x, y, z });
		break;
	}
	case 4: { //Make a new capsule shape
		break;
	}
	}

	if (toReturn) dpWorld::AddEntity(toReturn);

	return toReturn;
}

void PhysicsComponent::SpawnVertices(dpEntity* entity) const {
	if (!entity) return;

	LOG("Spawning vertices for %llu", m_Parent->GetObjectID());
	EntityInfo info;
	info.lot = 33;
	info.spawner = nullptr;
	info.spawnerID = m_Parent->GetObjectID();
	info.spawnerNodeID = 0;

	// These don't use overloaded methods as dPhysics does not link with dGame at the moment.
	auto box = dynamic_cast<dpShapeBox*>(entity->GetShape());
	if (box) {
		for (auto vert : box->GetVertices()) {
			LOG("Vertex at %f, %f, %f", vert.x, vert.y, vert.z);

			info.pos = vert;
			Entity* newEntity = Game::entityManager->CreateEntity(info);
			Game::entityManager->ConstructEntity(newEntity);
		}
	}
	auto sphere = dynamic_cast<dpShapeSphere*>(entity->GetShape());
	if (sphere) {
		auto [x, y, z] = entity->GetPosition(); // Use shapes position instead of the parent's position in case it's different
		float plusX = x + sphere->GetRadius();
		float minusX = x - sphere->GetRadius();
		float plusY = y + sphere->GetRadius();
		float minusY = y - sphere->GetRadius();
		float plusZ = z + sphere->GetRadius();
		float minusZ = z - sphere->GetRadius();

		auto radius = sphere->GetRadius();
		LOG("Radius: %f", radius);
		LOG("Plus Vertices %f %f %f", plusX, plusY, plusZ);
		LOG("Minus Vertices %f %f %f", minusX, minusY, minusZ);

		info.pos = NiPoint3{ x, plusY, z };
		Entity* newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ x, minusY, z };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ plusX, y, z };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ minusX, y, z };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ x, y, plusZ };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ x, y, minusZ };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);

		info.pos = NiPoint3{ x, y, z };
		newEntity = Game::entityManager->CreateEntity(info);
		Game::entityManager->ConstructEntity(newEntity);
	}
}
