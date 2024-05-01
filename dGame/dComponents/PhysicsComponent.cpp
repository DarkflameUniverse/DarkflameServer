#include "PhysicsComponent.h"

#include "eReplicaComponentType.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"

#include "dpEntity.h"

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

dpEntity* PhysicsComponent::CreatePhysicsEntity(const float scale) {
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	auto componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), eReplicaComponentType::PHANTOM_PHYSICS);

	CDPhysicsComponentTable* physComp = CDClientManager::GetTable<CDPhysicsComponentTable>();

	if (physComp == nullptr) return nullptr;

	auto* info = physComp->GetByID(componentID);
	if (info == nullptr || info->physicsAsset == "" || info->physicsAsset == "NO_PHYSICS") return nullptr;

	//temp test
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
		m_Position.y -= (111.467964f * scale) / 2;
	} else {
		// LOG_DEBUG("This one is supposed to have %s", info->physicsAsset.c_str());

		//add fallback cube:
		toReturn = new dpEntity(m_Parent->GetObjectID(), 2.0f, 2.0f, 2.0f);
	}
	return toReturn;
}
