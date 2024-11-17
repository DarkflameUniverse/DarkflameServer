#include "RocketLaunchpadControlComponent.h"

#include <sstream>

#include "GameMessages.h"
#include "CharacterComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Item.h"
#include "Game.h"
#include "Logger.h"
#include "CDClientDatabase.h"
#include "ChatPackets.h"
#include "MissionComponent.h"
#include "PropertyEntranceComponent.h"
#include "MultiZoneEntranceComponent.h"
#include "dServer.h"
#include "BitStreamUtils.h"
#include "eObjectWorldState.h"
#include "eConnectionType.h"
#include "eMasterMessageType.h"

RocketLaunchpadControlComponent::RocketLaunchpadControlComponent(Entity* parent, int rocketId) : Component(parent) {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT targetZone, defaultZoneID, targetScene, altLandingPrecondition, altLandingSpawnPointName FROM RocketLaunchpadControlComponent WHERE id = ?;");
	query.bind(1, rocketId);

	auto result = query.execQuery();

	if (!result.eof() && !result.fieldIsNull("targetZone")) {
		m_TargetZone = result.getIntField("targetZone");
		m_DefaultZone = result.getIntField("defaultZoneID");
		m_TargetScene = result.getStringField("targetScene");
		m_AltPrecondition = new PreconditionExpression(result.getStringField("altLandingPrecondition"));
		m_AltLandingScene = result.getStringField("altLandingSpawnPointName");
	}

	result.finalize();
}

RocketLaunchpadControlComponent::~RocketLaunchpadControlComponent() {
	delete m_AltPrecondition;
}

void RocketLaunchpadControlComponent::Launch(Entity* originator, LWOMAPID mapId, LWOCLONEID cloneId) {
	auto zone = mapId == LWOMAPID_INVALID ? m_TargetZone : mapId;

	if (zone == 0) {
		return;
	}

	// This also gets triggered by a proximity monitor + item equip, I will set that up when havok is ready
	auto* characterComponent = originator->GetComponent<CharacterComponent>();
	auto* character = originator->GetCharacter();

	if (!characterComponent || !character) return;

	auto* rocket = characterComponent->GetRocket(originator);
	if (!rocket) {
		LOG("Unable to find rocket!");
		return;
	}

	// we have the ability to launch, so now we prep the zone
	TellMasterToPrepZone(zone);

	// Achievement unlocked: "All zones unlocked"
	if (!m_AltLandingScene.empty() && m_AltPrecondition->Check(originator)) {
		character->SetTargetScene(m_AltLandingScene);
	} else {
		character->SetTargetScene(m_TargetScene);
	}

	characterComponent->UpdatePlayerStatistic(RocketsUsed);

	character->SaveXMLToDatabase();

	SetSelectedMapId(originator->GetObjectID(), zone);

	GameMessages::SendFireEventClientSide(m_Parent->GetObjectID(), originator->GetSystemAddress(), u"RocketEquipped", rocket->GetId(), cloneId, -1, originator->GetObjectID());

	GameMessages::SendChangeObjectWorldState(rocket->GetId(), eObjectWorldState::ATTACHED, UNASSIGNED_SYSTEM_ADDRESS);

	Game::entityManager->SerializeEntity(originator);
}

void RocketLaunchpadControlComponent::OnUse(Entity* originator) {
	// If we are have the property or the LUP component, we don't want to immediately launch
	// instead we let their OnUse handlers do their things
	// which components of an Object have their OnUse called when using them
	// so we don't need to call it here
	auto* propertyEntrance = m_Parent->GetComponent<PropertyEntranceComponent>();
	if (propertyEntrance) {
		return;
	}

	auto* rocketLaunchLUP = m_Parent->GetComponent<MultiZoneEntranceComponent>();
	if (rocketLaunchLUP) {
		return;
	}

	// No rocket no launch
	auto* rocket = originator->GetComponent<CharacterComponent>()->RocketEquip(originator);
	if (!rocket) {
		return;
	}
	Launch(originator);
}

void RocketLaunchpadControlComponent::OnProximityUpdate(Entity* entering, std::string name, std::string status) {
	// Proximity rockets are handled by item equipment
}

void RocketLaunchpadControlComponent::SetSelectedMapId(LWOOBJID player, LWOMAPID mapID) {
	m_SelectedMapIds[player] = mapID;
}

LWOMAPID RocketLaunchpadControlComponent::GetSelectedMapId(LWOOBJID player) const {
	const auto index = m_SelectedMapIds.find(player);

	if (index == m_SelectedMapIds.end()) return 0;

	return index->second;
}

void RocketLaunchpadControlComponent::SetSelectedCloneId(LWOOBJID player, LWOCLONEID cloneId) {
	m_SelectedCloneIds[player] = cloneId;
}

LWOCLONEID RocketLaunchpadControlComponent::GetSelectedCloneId(LWOOBJID player) const {
	const auto index = m_SelectedCloneIds.find(player);

	if (index == m_SelectedCloneIds.end()) return 0;

	return index->second;
}

void RocketLaunchpadControlComponent::TellMasterToPrepZone(int zoneID) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::MASTER, eMasterMessageType::PREP_ZONE);
	bitStream.Write(zoneID);
	Game::server->SendToMaster(bitStream);
}


LWOMAPID RocketLaunchpadControlComponent::GetTargetZone() const {
	return m_TargetZone;
}

LWOMAPID RocketLaunchpadControlComponent::GetDefaultZone() const {
	return m_DefaultZone;
}
