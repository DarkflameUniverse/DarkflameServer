#include "RocketLaunchpadControlComponent.h"

#include <sstream>

#include "GameMessages.h"
#include "CharacterComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Item.h"
#include "Game.h"
#include "dLogger.h"
#include "CDClientDatabase.h"
#include "ChatPackets.h"
#include "MissionComponent.h"
#include "PropertyEntranceComponent.h"
#include "dServer.h"
#include "dMessageIdentifiers.h"
#include "PacketUtils.h"

RocketLaunchpadControlComponent::RocketLaunchpadControlComponent(Entity* parent, int rocketId) : Component(parent) {
	auto result = CDClientDatabase::ExecuteQueryWithArgs(
		"SELECT targetZone, defaultZoneID, targetScene, altLandingPrecondition, altLandingSpawnPointName FROM RocketLaunchpadControlComponent WHERE id = %d;",
		rocketId);

	if (!result.eof() && !result.fieldIsNull(0))
	{
		m_TargetZone = result.getIntField(0);
		m_DefaultZone = result.getIntField(1);
		m_TargetScene = result.getStringField(2);
		m_AltPrecondition = new PreconditionExpression(result.getStringField(3));
		m_AltLandingScene = result.getStringField(4);
	}

	result.finalize();
}

RocketLaunchpadControlComponent::~RocketLaunchpadControlComponent() {
	delete m_AltPrecondition;
}

void RocketLaunchpadControlComponent::RocketEquip(Entity* entity, LWOOBJID rocketID) {
	if (m_PlayersInRadius.find(entity->GetObjectID()) != m_PlayersInRadius.end()) {
		Launch(entity, rocketID);

		//Go ahead and save the player
		//This causes a double-save, but it should prevent players from not being saved
		//before the next world server starts loading their data.
		if (entity->GetCharacter())
			entity->GetCharacter()->SaveXMLToDatabase();
	}
}

void RocketLaunchpadControlComponent::Launch(Entity* originator, LWOOBJID optionalRocketID, LWOMAPID mapId, LWOCLONEID cloneId) {
	auto zone = mapId == LWOMAPID_INVALID ? m_TargetZone : mapId;

	if (zone == 0)
	{
		return;
	}
	
	TellMasterToPrepZone(zone);

	// This also gets triggered by a proximity monitor + item equip, I will set that up when havok is ready
	auto* inventoryComponent = originator->GetComponent<InventoryComponent>();
	auto* characterComponent = originator->GetComponent<CharacterComponent>();

	auto* character = originator->GetCharacter();

	if (inventoryComponent == nullptr || characterComponent == nullptr || character == nullptr) {
		return;
	}

	// Select the rocket

	Item* rocket = nullptr;

	if (optionalRocketID != LWOOBJID_EMPTY)
	{
		rocket = inventoryComponent->FindItemById(optionalRocketID);
	}
	
	if (rocket == nullptr)
	{
		rocket = inventoryComponent->FindItemById(characterComponent->GetLastRocketItemID());
	}

	if (rocket == nullptr)
	{
		rocket = inventoryComponent->FindItemByLot(6416);
	}

	if (rocket == nullptr)
	{
		Game::logger->Log("RocketLaunchpadControlComponent", "Unable to find rocket (%llu)!\n", optionalRocketID);

		return;
	}

	if (rocket->GetConfig().empty()) // Sanity check
	{
		rocket->SetCount(0, false, false);

		return;
	}

	// Achievement unlocked: "All zones unlocked"

	if (!m_AltLandingScene.empty() && m_AltPrecondition->Check(originator)) {
		character->SetTargetScene(m_AltLandingScene);
	}
	else {
		character->SetTargetScene(m_TargetScene);
	}

	if (characterComponent) {
		for (LDFBaseData* data : rocket->GetConfig()) {
			if (data->GetKey() == u"assemblyPartLOTs") {
				std::string newRocketStr;
				for (char character : data->GetValueAsString()) {
					if (character == '+') {
						newRocketStr.push_back(';');
					}
					else {
						newRocketStr.push_back(character);
					}
				}
				newRocketStr.push_back(';');
				characterComponent->SetLastRocketConfig(GeneralUtils::ASCIIToUTF16(newRocketStr));
			}
		}
	}
	
	// Store the last used rocket item's ID
	characterComponent->SetLastRocketItemID(rocket->GetId());

	characterComponent->UpdatePlayerStatistic(RocketsUsed);

	character->SaveXMLToDatabase();

	SetSelectedMapId(originator->GetObjectID(), zone);
	
	GameMessages::SendFireEventClientSide(m_Parent->GetObjectID(), originator->GetSystemAddress(), u"RocketEquipped", rocket->GetId(), cloneId, -1, originator->GetObjectID());

	rocket->Equip(true);
	
	GameMessages::SendChangeObjectWorldState(rocket->GetId(), WORLDSTATE_ATTACHED, UNASSIGNED_SYSTEM_ADDRESS);
	
	EntityManager::Instance()->SerializeEntity(originator);
}

void RocketLaunchpadControlComponent::OnUse(Entity* originator) {
	auto* propertyEntrance = m_Parent->GetComponent<PropertyEntranceComponent>();

	if (propertyEntrance != nullptr)
	{
		propertyEntrance->OnUse(originator);

		return;
	}

	Launch(originator);
}

void RocketLaunchpadControlComponent::OnProximityUpdate(Entity* entering, std::string name, std::string status) {
	// Proximity rockets are handled by item equipment
}

void RocketLaunchpadControlComponent::SetSelectedMapId(LWOOBJID player, LWOMAPID mapID) 
{
	m_SelectedMapIds[player] = mapID;
}

LWOMAPID RocketLaunchpadControlComponent::GetSelectedMapId(LWOOBJID player) const
{
	const auto index = m_SelectedMapIds.find(player);

	if (index == m_SelectedMapIds.end()) return 0;

	return index->second;
}

void RocketLaunchpadControlComponent::SetSelectedCloneId(LWOOBJID player, LWOCLONEID cloneId) 
{
	m_SelectedCloneIds[player] = cloneId;
}

LWOCLONEID RocketLaunchpadControlComponent::GetSelectedCloneId(LWOOBJID player) const
{
	const auto index = m_SelectedCloneIds.find(player);

	if (index == m_SelectedCloneIds.end()) return 0;

	return index->second;
}

void RocketLaunchpadControlComponent::TellMasterToPrepZone(int zoneID) {
	CBITSTREAM;
	PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_PREP_ZONE);
	bitStream.Write(zoneID);
	Game::server->SendToMaster(&bitStream);
}


LWOMAPID RocketLaunchpadControlComponent::GetTargetZone() const
{
	return m_TargetZone;
}

LWOMAPID RocketLaunchpadControlComponent::GetDefaultZone() const
{
	return m_DefaultZone;
}
