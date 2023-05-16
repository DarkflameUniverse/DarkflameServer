#include "PropertyTeleportBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ChatPackets.h"
#include "WorldPackets.h"
#include "EntityManager.h"
#include "Game.h"
#include "ZoneInstanceManager.h"
#include "dZoneManager.h"

void PropertyTeleportBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* caster = EntityManager::Instance()->GetEntity(context->caster);
	if (!caster) return;

	auto* character = caster->GetCharacter();
	if (!character) return;

	LWOOBJID objId = caster->GetObjectID();

	LWOMAPID targetMapId = m_MapId;
	LWOCLONEID targetCloneId = character->GetPropertyCloneID();

	if (dZoneManager::Instance()->GetZoneID().GetCloneID() == character->GetPropertyCloneID()) {
		targetMapId = character->GetLastNonInstanceZoneID();
		targetCloneId = 0;
	} else {
		character->SetLastNonInstanceZoneID(dZoneManager::Instance()->GetZoneID().GetMapID());
	}

	ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, targetMapId, targetCloneId, false, [objId](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {

		auto* entity = EntityManager::Instance()->GetEntity(objId);
		if (!entity) return;

		const auto sysAddr = entity->GetSystemAddress();

		if (zoneClone != 0) ChatPackets::SendSystemMessage(sysAddr, u"Transfering to your property!");
		else ChatPackets::SendSystemMessage(sysAddr, u"Transfering back to previous world!");

		Log("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", sysAddr.ToString(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);
		if (entity->GetCharacter()) {
			entity->GetCharacter()->SetZoneID(zoneID);
			entity->GetCharacter()->SetZoneInstance(zoneInstance);
			entity->GetCharacter()->SetZoneClone(zoneClone);
			entity->GetComponent<CharacterComponent>()->SetLastRocketConfig(u"");
		}

		entity->GetCharacter()->SaveXMLToDatabase();

		WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
		return;
		});
}

void PropertyTeleportBehavior::Load() {
	this->m_CancelIfInteracting = GetBoolean("cancel_if_interacting"); // TODO unused
	this->m_MapId = LWOMAPID(GetInt("mapID"));
}
